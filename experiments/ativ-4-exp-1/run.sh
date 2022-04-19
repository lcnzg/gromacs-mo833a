#!/usr/bin/env bash

GMX="../../build-release/bin/gmx"

./compile.sh

./download.sh

if [[ ! -f "em.tpr" ]]; then
    $GMX pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce -ff oplsaa
    $GMX editconf -f 6LVN_processed.gro -o 6LVN_newbox.gro -c -d 1.0 -bt cubic
    $GMX solvate -cp 6LVN_newbox.gro -cs spc216.gro -o 6LVN_solv.gro -p topol.top
    $GMX grompp -f ions.mdp -c 6LVN_solv.gro -p topol.top -o ions.tpr
    echo "SOL" | $GMX genion -s ions.tpr -o 6LVN_solv_ions.gro -p topol.top -pname NA -nname CL -neutral
    $GMX grompp -f ions.mdp -c 6LVN_solv_ions.gro -p topol.top -o em.tpr
fi

perf record --call-graph dwarf $GMX mdrun -v -deffnm em
valgrind --tool=callgrind $GMX mdrun -v -deffnm em

# perf report 
# callgrind_annotate callgrind.out.*
