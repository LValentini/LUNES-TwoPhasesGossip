set terminal postscript enhanced color solid
set size 0.8,0.8
set output "graph_overhead_coverage_degree_dependent.eps"
set title "Dissemination protocol comparison: coverage"
set xlabel "Overhead ratio ({/Symbol r})"
set ylabel "Coverage (%)"
set key right bottom
set yrange [0:103]

plot \
"ddf2.dat"		using 5:2   title "degree dependent gossip (DDF2)" with points pointtype 2 pointsize 0.5 lc 2, \
"ddf1.dat"		using 5:2	title "degree dependent gossip (DDF1)" with points pointtype 3 pointsize 0.5 lc 1

show key
show xlabel
show ylabel
show output
