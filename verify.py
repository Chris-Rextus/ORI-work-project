#!/usr/bin/env python3
"""
verify.py - cross-check the ORI collaboration-graph program against an
independent ground truth computed in Python.

Usage:
    python3 verify.py [datafile] [source.c]

Defaults: dadosPesquisadores.txt  GrupoPYP.cpp

It parses the data file exactly the way the C program does (split on the first
tab, trim ' \\t\\r\\n' from both ends, skip empty name/title), rebuilds the graph,
then compiles and drives the C program and diffs every operation. Exit code is
0 only if all checks pass.
"""
import sys, os, re, subprocess, tempfile

WS = b" \t\r\n"
GARBAGE = b"zzzz pesquisador inexistente zzzz"


# ---------------------------------------------------------------- ground truth
def build_truth(path):
    names = {}          # bytes -> id
    id_names = []       # id -> bytes
    title_authors = {}  # bytes -> set(id)

    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(b"\xef\xbb\xbf"):        # strip UTF-8 BOM (file start)
        data = data[3:]

    for raw in data.split(b"\n"):
        p = raw.find(b"\t")
        if p < 0:
            continue
        name = raw[:p].strip(WS)
        title = raw[p + 1:].strip(WS)
        if not name or not title:
            continue
        nid = names.get(name)
        if nid is None:
            nid = len(id_names)
            names[name] = nid
            id_names.append(name)
        title_authors.setdefault(title, set()).add(nid)

    N = len(id_names)
    neighbors = [set() for _ in range(N)]
    for authors in title_authors.values():
        if len(authors) < 2:
            continue
        al = list(authors)
        for i in range(len(al)):
            for j in range(i + 1, len(al)):
                neighbors[al[i]].add(al[j])
                neighbors[al[j]].add(al[i])

    degs = [len(n) for n in neighbors]
    E = sum(degs) // 2
    maxdeg = max(degs) if degs else 0
    hub = degs.index(maxdeg) if degs else -1
    avg = (2.0 * E) / N if N else 0.0

    return {
        "names": names, "id_names": id_names, "title_authors": title_authors,
        "neighbors": neighbors, "degs": degs,
        "N": N, "T": len(title_authors), "E": E,
        "maxdeg": maxdeg, "hub": hub, "avg": avg,
    }


def pick_queries(g):
    hub = g["hub"]
    id_names = g["id_names"]

    # clean (no embedded tab) title with the most authors -> option 2
    best_t, best_k = None, -1
    for t, a in g["title_authors"].items():
        if b"\t" in t:
            continue
        if len(a) > best_k:
            best_t, best_k = t, len(a)

    # hub's collaborator sharing the most distinct titles -> positive option 5
    share = {}
    for t, a in g["title_authors"].items():
        if hub in a:
            for x in a:
                if x != hub:
                    share[x] = share.get(x, 0) + 1
    topcollab = max(share, key=share.get) if share else None

    # someone who is NOT a collaborator of the hub -> negative option 5
    neigh = g["neighbors"][hub] if hub >= 0 else set()
    noncollab = None
    for i in range(g["N"]):
        if i != hub and i not in neigh:
            noncollab = i
            break

    return {
        "hub_name": id_names[hub] if hub >= 0 else b"",
        "hub_deg": g["maxdeg"],
        "title": best_t, "title_authors": best_k,
        "topcollab": id_names[topcollab] if topcollab is not None else None,
        "topcollab_shared": share.get(topcollab, 0) if topcollab is not None else 0,
        "noncollab": id_names[noncollab] if noncollab is not None else None,
    }


# ------------------------------------------------------------------- run C prog
def compile_source(src):
    out = tempfile.mktemp(prefix="ori_verify_")
    r = subprocess.run(["gcc", "-x", "c", "-O2", "-std=c11", "-o", out, src],
                       stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("COMPILE FAILED:\n" + r.stderr.decode(errors="replace"))
        sys.exit(2)
    return out


def drive(binary, datafile, q):
    lines = [b"3", b"4", b"1", q["hub_name"], b"2", q["title"]]
    if q["topcollab"] is not None:
        lines += [b"5", q["hub_name"], q["topcollab"]]
    if q["noncollab"] is not None:
        lines += [b"5", q["hub_name"], q["noncollab"]]
    lines += [b"1", GARBAGE, b"0"]
    stdin = b"\n".join(lines) + b"\n"
    r = subprocess.run([binary, datafile], input=stdin,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=600)
    return r.stdout.decode(errors="replace")


def main():
    datafile = sys.argv[1] if len(sys.argv) > 1 else "dadosPesquisadores.txt"
    source = sys.argv[2] if len(sys.argv) > 2 else "GrupoPYP.cpp"

    print(f"data   : {datafile}")
    print(f"source : {source}")
    print("computing ground truth ...")
    g = build_truth(datafile)
    q = pick_queries(g)

    print("compiling ...")
    binary = compile_source(source)
    print("running program ...\n")
    out = drive(binary, datafile, q)
    os.unlink(binary)

    def grab(pat, default=None):
        m = re.search(pat, out)
        return m.group(1) if m else default

    prog = {
        "N":   grab(r"Pesquisadores:\s*(\d+)"),
        "T":   grab(r"Titulos unicos:\s*(\d+)"),
        "E":   grab(r"Arestas de colaboracao:\s*(\d+)"),
        "max": grab(r"Maior grau:\s*(\d+)"),
        "avg": grab(r"Grau medio:\s*([\d.]+)"),
        "col": grab(r"Total:\s*(\d+)\s*colaborador"),
        "aut": grab(r"Total:\s*(\d+)\s*autor"),
        "shr": grab(r"Total:\s*(\d+)\s*titulo"),
    }
    neg_ok = "nao sao colaboradores" in out
    garb_ok = "nao encontrado" in out

    checks = [
        ("researchers (banner)",        str(g["N"]),            prog["N"]),
        ("unique titles (banner)",      str(g["T"]),            prog["T"]),
        ("edges (banner)",              str(g["E"]),            prog["E"]),
        ("max degree (opt 3)",          str(g["maxdeg"]),       prog["max"]),
        ("avg degree (opt 4)",          f"{g['avg']:.2f}",      prog["avg"]),
        ("hub collaborators (opt 1)",   str(g["maxdeg"]),       prog["col"]),
        ("authors of title (opt 2)",    str(q["title_authors"]), prog["aut"]),
    ]
    if q["topcollab"] is not None:
        checks.append(("shared titles + (opt 5)", str(q["topcollab_shared"]), prog["shr"]))
    checks.append(("shared titles - (opt 5)", "not-collab", "not-collab" if neg_ok else "MISSING"))
    checks.append(("unknown name (opt 1)",     "not-found",  "not-found"  if garb_ok else "MISSING"))

    print(f"hub           : {q['hub_name'].decode(errors='replace')}  (deg {q['hub_deg']})")
    print(f"probe title   : {q['title'].decode(errors='replace')!r}  ({q['title_authors']} authors)")
    if q["topcollab"] is not None:
        print(f"top collab    : {q['topcollab'].decode(errors='replace')}  ({q['topcollab_shared']} shared)")
    if q["noncollab"] is not None:
        print(f"non-collab    : {q['noncollab'].decode(errors='replace')}")
    print()

    width = max(len(c[0]) for c in checks)
    allok = True
    for label, exp, got in checks:
        ok = (exp == got)
        allok &= ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {label:<{width}}  expected={exp:<12} program={got}")

    print()
    print("RESULT:", "ALL CHECKS PASSED" if allok else "SOME CHECKS FAILED")
    sys.exit(0 if allok else 1)


if __name__ == "__main__":
    main()