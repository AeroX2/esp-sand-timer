$fn = 50;
eps = 0.01;

p10_width = 192;
p10_height = 96.6;

border_size = 5;

hole_size = 2.4;
hole_gap = 0.6;
hole_z_gap = 2;

grid();

module grid() {
    translate([-hole_size/2,-hole_size/2,0]) {
        union() {
            t = hole_size+hole_gap;
            z = hole_z_gap;
            for (i = [0: 64]) {
                translate([i*t,-eps,0]) cube([hole_gap,p10_height+eps*2,z]);
            }
            for (i = [0: 32]) {
                translate([-eps,i*t,0]) cube([p10_width+eps*2,hole_gap,z]);
            }
        }
    }
}