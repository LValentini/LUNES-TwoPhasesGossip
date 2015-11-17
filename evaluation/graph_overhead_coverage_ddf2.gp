set terminal postscript enhanced color solid
set size 0.8,0.8
set output "graph_overhead_coverage_ddf2.eps"
set title "Dissemination protocol comparison: coverage"
set xlabel "Overhead ratio ({/Symbol r})"
set ylabel "Coverage (%)"
set key right bottom
set yrange [0:103]

plot \
"ddf2.dat"		using 5:2       title "degree dependent gossip (DDF2)" with points pointtype 4 pointsize 0.5 lc 3, \
"fixed.dat"		using 5:2       title "fixed probability" with points pointtype 2 pointsize 0.5 lc 2, \
"broadcast.dat"		using 5:2	title "probabilistic broadcast" with points pointtype 3 pointsize 0.5 lc 1

show key
show xlabel
show ylabel
show output
