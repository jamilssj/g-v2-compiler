#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPILER="$ROOT_DIR/g-v2"

run_dir() {
  local dir="$1"
  local expect_fail="$2"
  local total=0
  local failed=0
  local file

  shopt -s nullglob
  for file in "$dir"/*.g "$dir"/*.txt; do
    total=$((total + 1))
    if "$COMPILER" "$file" >/dev/null 2>&1; then
      if [[ "$expect_fail" == "1" ]]; then
        echo "FALHOU: esperava erro e compilou: $file"
        failed=$((failed + 1))
      fi
    else
      if [[ "$expect_fail" == "0" ]]; then
        echo "FALHOU: esperava sucesso e falhou: $file"
        failed=$((failed + 1))
      fi
    fi
  done

  echo "Diretorio: $dir | total=$total | falhas=$failed"
  if [[ "$failed" -ne 0 ]]; then
    return 1
  fi
}

run_dir "$ROOT_DIR/examples/parte2/Corretos" 0
run_dir "$ROOT_DIR/examples/parte2/ErrosSemanticos" 1

echo "Todos os testes passaram."
