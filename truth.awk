{
  line=$0
  if(NR==1 && substr(line,1,3)=="\357\273\277") line=substr(line,4)
  p=index(line,"\t"); if(p==0) next
  name=substr(line,1,p-1); title=substr(line,p+1)
  gsub(/^[ \t]+|[ \t\r\n]+$/,"",name); gsub(/^[ \t]+|[ \t\r\n]+$/,"",title)
  if(name==""||title=="") next
  if(!((title SUBSEP name) in seen)){ seen[title SUBSEP name]=1; a[title]=a[title] SUBSEP name }
  if(!(name in known)){ known[name]=1; N++ }
  T[title]=1
}
END{
  for(t in T) uT++
  for(t in a){ n=split(a[t],arr,SUBSEP); m=0; split("",ord)
    for(i=1;i<=n;i++) if(arr[i]!="") ord[++m]=arr[i]
    for(i=1;i<=m;i++) for(j=i+1;j<=m;j++){ p=ord[i]; q=ord[j]
      ek=(p<q)?p SUBSEP q:q SUBSEP p
      if(!(ek in edge)){ edge[ek]=1; deg[p]++; deg[q]++; E++ } } }
  mx=0; for(p in deg) if(deg[p]>mx){ mx=deg[p]; who=p }
  printf "researchers=%d\nunique titles=%d\nedges=%d\nmax degree=%d (%s)\navg degree=%.2f\n", N,uT,E,mx,who,(2.0*E)/N
}
