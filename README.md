# Trabalho ORI — Grafo de Colaboração de Pesquisadores

Disciplina: Organização e Recuperação da Informação
Entrega: 07/07/2026 (por e-mail, assunto "Trabalho ORI")

## Integrantes
- Fulano de Tal — matrícula
- Ciclano de Tal — matrícula
- Beltrano de Tal — matrícula

## O que o programa faz
Lê um arquivo `nome\ttítulo` e constrói:
- Hash de nomes (nome → IdP)
- Índice IdP → nome
- Hash de títulos (detecta colisão tradicional vs. colisão repetida = colaboração)
- Grafo de colaboração (aresta = lista de títulos em comum)

Operações suportadas:
1. Listar colaboradores de um pesquisador
2. Listar autores de um título
3. Maior grau do grafo
4. Grau médio do grafo

## Build (desenvolvimento)
```bash
gcc -Wall -Wextra -std=c11 -o ori src/main.c
./ori dados.txt
```

## Build com testes (opcional, dev only)
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## Entrega final
Antes de enviar, copiar **apenas** `src/main.c` para `GrupoXXX.c`
(sem dependências externas, sem CMake, sem vcpkg — só stdlib).