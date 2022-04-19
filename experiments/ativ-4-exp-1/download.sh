#!/usr/bin/env bash

if [[ ! -f "6LVN.pdb" ]]; then
    wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/6LVN.pdb
fi
if [[ ! -f "ions.mdp" ]]; then
    wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/ions.mdp
fi
