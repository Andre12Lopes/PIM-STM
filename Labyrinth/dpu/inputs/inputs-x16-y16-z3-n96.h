#ifndef _INPUTS_H_
#define _INPUTS_H_

#define NUM_PATHS 96
#define RANGE_X 16
#define RANGE_Y 16
#define RANGE_Z 3

__mram int PATHS[NUM_PATHS][6] =
{
{12,13,0,8,15,1},
{9,15,1,6,4,1},
{4,3,2,8,4,1},
{3,2,2,10,15,2},
{3,11,1,10,6,2},
{15,14,2,8,1,2},
{0,2,2,12,0,2},
{15,10,0,10,2,0},
{7,7,0,14,2,0},
{10,15,0,9,9,2},
{3,10,2,6,9,1},
{2,12,1,7,9,0},
{6,5,0,8,15,0},
{2,4,0,1,2,2},
{12,8,2,7,6,2},
{13,8,1,15,11,0},
{10,3,1,10,6,0},
{0,8,0,7,11,0},
{10,13,0,3,4,2},
{7,1,2,2,0,0},
{6,3,1,2,11,0},
{1,0,0,5,3,1},
{6,1,2,0,13,2},
{3,8,0,7,2,2},
{9,11,1,5,1,2},
{14,1,2,3,12,0},
{8,11,2,15,5,2},
{6,1,2,5,5,1},
{8,3,2,14,5,0},
{15,13,2,9,11,1},
{8,4,2,0,14,2},
{2,10,2,1,8,0},
{7,15,1,9,11,2},
{4,9,1,13,2,0},
{6,10,0,7,7,2},
{14,12,2,13,1,1},
{13,1,0,14,2,1},
{5,14,2,15,0,0},
{15,10,1,14,1,1},
{6,2,2,4,0,1},
{13,10,0,6,0,2},
{0,3,0,3,6,1},
{8,5,0,15,12,2},
{2,0,1,14,3,1},
{4,11,0,4,8,0},
{1,1,0,8,10,1},
{1,15,2,14,13,1},
{5,6,1,9,0,0},
{4,8,1,10,11,2},
{2,10,2,1,1,1},
{5,4,2,9,11,1},
{4,9,0,15,7,0},
{9,5,2,2,9,1},
{10,9,1,3,3,2},
{15,15,1,10,3,1},
{3,15,1,1,9,1},
{4,5,2,12,2,0},
{2,6,2,7,1,1},
{0,3,1,9,14,1},
{6,13,0,11,7,1},
{5,13,0,11,3,0},
{0,14,2,6,3,1},
{12,8,0,1,6,2},
{4,3,0,14,12,1},
{4,3,2,15,4,2},
{12,13,2,15,10,1},
{15,6,2,7,0,1},
{10,10,0,4,8,2},
{4,12,2,9,15,0},
{2,1,0,7,4,0},
{9,0,1,10,5,0},
{14,11,2,12,1,2},
{2,2,2,13,6,1},
{13,15,1,7,0,2},
{0,5,1,8,10,0},
{15,8,1,13,12,1},
{1,5,2,4,7,1},
{10,1,0,15,13,0},
{15,2,2,4,11,1},
{1,14,1,14,1,0},
{15,4,0,1,4,2},
{10,3,2,11,6,1},
{15,3,0,14,10,2},
{3,9,0,12,9,2},
{3,6,0,12,14,1},
{6,14,1,2,1,0},
{15,8,0,6,7,0},
{13,9,0,4,13,2},
{13,2,0,13,4,2},
{0,14,1,13,0,1},
{10,8,0,11,2,0},
{11,0,1,11,5,0},
{7,11,0,4,6,0},
{6,3,2,0,9,1},
{0,7,0,5,14,0},
{15,11,2,8,4,0},
};

// __mram int PATHS[1][6] = {
//     {0, 0, 1, 5, 5, 1},
// };

// __mram int PATHS[1][6] = {
//     {0, 0, 1, 5, 5, 1},
// };

// cat file.txt | tr -s ' ' ", #" | cut -d ', ' -f 2-7 | awk '{print "{"$0"},"}'

#endif /* _INPUTS_H_ */
