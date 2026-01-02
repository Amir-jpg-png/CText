#!/usr/bin/env bash

fatal (){
  echo '[fatal]' "$@" >&2
  exit 1
}

main (){
  curl -S https://raw.githubusercontent.com/Amir-jpg-png/CText/refs/heads/main/ctext.c > to_compile.c || fatal 'failed to pull source code from remote repository'
  ${CC:-cc} to_compile.c -o ctext -Wall -Wextra -pedantic -std=c99 \
    || fatal 'failed to compile from source'
  rm -f to_compile.c
  read -r -p "Do you want to make ctext a global executable? (y/n): " choice

  if [[ $choice == 'y' ]]; then
    sudo mv ctext /usr/local/bin/;
  else
    echo "The compiled executable is is in: $PWD"
  fi
}

main
