set terminal png size 800,400
set output "scan_hist.png"
set title "Best-level scan latency"
set xlabel "nanoseconds"
binwidth=10
bin(x,width)=width*floor(x/width)
plot "scan_ns.txt" using (bin($1,binwidth)):(1.0) smooth freq with boxes lc rgb"#406090" 