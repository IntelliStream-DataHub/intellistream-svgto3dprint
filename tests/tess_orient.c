/* Probe libtess2 output orientation. */
#include "../src/region.h"
#include <stdio.h>
int main(void)
{
    region_t in, out; int i;
    double outer[] = {0,0, 10,0, 10,10, 0,10};       /* CCW */
    double hole[]  = {3,3, 7,3, 7,7, 3,7};           /* CCW too */
    double outer_cw[] = {0,0, 0,10, 10,10, 10,0};    /* CW */
    region_init(&in);
    region_add_contour(&in, outer, 4);
    region_add_contour(&in, hole, 4);
    region_normalize(&out, &in, 1);
    printf("evenodd, both CCW: %d contours\n", out.n);
    for (i = 0; i < out.n; i++) printf("  contour %d: n=%d area=%.2f\n", i, out.c[i].n, contour_area(&out.c[i]));
    printf("  region area=%.2f\n", region_area(&out));
    region_free(&out); region_free(&in);
    region_init(&in);
    region_add_contour(&in, outer_cw, 4);
    region_normalize(&out, &in, 0);
    printf("nonzero, CW input: %d contours\n", out.n);
    for (i = 0; i < out.n; i++) printf("  contour %d: n=%d area=%.2f\n", i, out.c[i].n, contour_area(&out.c[i]));
    /* subtract test: square minus smaller square */
    {
        region_t a, b, na, nb, d; const region_t *subs[1];
        double sq[] = {0,0, 10,0, 10,10, 0,10};
        double sm[] = {5,5, 15,5, 15,15, 5,15};
        region_init(&a); region_add_contour(&a, sq, 4); region_normalize(&na, &a, 0);
        region_init(&b); region_add_contour(&b, sm, 4); region_normalize(&nb, &b, 0);
        subs[0] = &nb;
        region_subtract(&d, &na, subs, 1);
        printf("subtract: %d contours, area=%.2f (expect 75)\n", d.n, region_area(&d));
        for (i = 0; i < d.n; i++) printf("  contour %d: n=%d area=%.2f\n", i, d.c[i].n, contour_area(&d.c[i]));
        {
            double *v; int nv, *t, nt;
            region_triangulate(&d, &v, &nv, &t, &nt);
            printf("triangulate: %d verts %d tris\n", nv, nt);
            {
                double s = 0; int k;
                for (k = 0; k < nt; k++) {
                    double ax = v[2*t[3*k]], ay = v[2*t[3*k]+1], bx = v[2*t[3*k+1]], by = v[2*t[3*k+1]+1], cx = v[2*t[3*k+2]], cy = v[2*t[3*k+2]+1];
                    s += 0.5 * ((bx-ax)*(cy-ay) - (cx-ax)*(by-ay));
                }
                printf("  signed tri area sum=%.2f\n", s);
            }
        }
    }
    return 0;
}
