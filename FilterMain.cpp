#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int main(int argc, char **argv)
{
    if(argc < 2) 
    {
        fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
    }

    //
    // Convert to C++ strings to simplify manipulation
    //
    string filtername = argv[1];

    //
    // remove any ".filter" in the filtername
    //
    string filterOutputName = filtername;
    string::size_type loc = filterOutputName.find(".filter");
    if (loc != string::npos) 
    {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
        filterOutputName = filtername.substr(0, loc);
    }

    Filter *filter = readFilter(filtername);

    double sum = 0.0;
    int samples = 0;
    
    for (int inNum = 2; inNum < argc; inNum++) 
    {
        string inputFilename = argv[inNum];
        string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
        struct cs1300bmp *input = new struct cs1300bmp;
        struct cs1300bmp *output = new struct cs1300bmp;
        int ok = cs1300bmp_readfile((char *) inputFilename.c_str(), input);

        if(ok) 
        {
            double sample = applyFilter(filter, input, output);
            sum += sample;
            samples++;
            cs1300bmp_writefile((char *) outputFilename.c_str(), output);
        }
        delete input;
        delete output;
    }
    fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);
}

struct Filter *readFilter(string filename)
{
    ifstream input(filename.c_str());

    if(!input.bad()) 
    {
        int size = 0;
        input >> size;
        Filter *filter = new Filter(size);
        int div;
        input >> div;
        filter->setDivisor(div);
        for (int i=0; i < size; i++) 
        {
            for (int j=0; j < size; j++)
            {
                int value;
                input >> value;
                filter->set(i, j, value);
            }
        }
        return filter;
    }
    else 
    {
        cerr << "Bad input in readFilter:" << filename << endl;
        exit(-1);
    }
}


double applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

    long long cycStart, cycStop;

    cycStart = rdtscll();
    output->width = input->width;
    output->height = input->height;
    
    int divisor = filter->getDivisor();
    
    int rowMax = input->height - 1;
    int colMax = input->width - 1;
    
    int arrGet[9];
    arrGet[0] = filter->get(0, 0);
    arrGet[1] = filter->get(0, 1);
    arrGet[2] = filter->get(0, 2);
    arrGet[3] = filter->get(1, 0);
    arrGet[4] = filter->get(1, 1);
    arrGet[5] = filter->get(1, 2);
    arrGet[6] = filter->get(2, 0);
    arrGet[7] = filter->get(2, 1);
    arrGet[8] = filter->get(2, 2);
    

// change to row++ and col++

    #pragma omp parallel for
    for(int row = 1; row < rowMax; row++) 
    {
        int rowUp = row + 1;
        int rowDown = row - 1;
        
        for(int col = 1; col < colMax; col++) 
        {
            int colUp = col + 1;
            int colDown = col - 1;
 
            for(int plane = 0; plane < 3; plane++) 
            {
                output->color[row][col][plane] = 0;
// Unrolling loop for i and j
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowDown][colDown][plane] * arrGet[0]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowDown][col][plane] * arrGet[1]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowDown][colUp][plane] * arrGet[2]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[row][colDown][plane] * arrGet[3]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[row][col][plane] * arrGet[4]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[row][colUp][plane] * arrGet[5]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowUp][colDown][plane] * arrGet[6]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowUp][col][plane] * arrGet[7]);
                output->color[row][col][plane] = output->color[row][col][plane] + (input->color[rowUp][colUp][plane] * arrGet[8]);  
                
// **Remove function call getDivisor()	
                output->color[row][col][plane] = output->color[row][col][plane] / divisor;

                if(output->color[row][col][plane]  < 0) 
                {
                    output->color[row][col][plane] = 0;
                }

                if(output->color[row][col][plane]  > 255) 
                { 
                    output->color[row][col][plane] = 255;
                }
            }
        }
    }

    int area = output->width * output->height;
    
    cycStop = rdtscll();
    double diff = cycStop - cycStart;
    double diffPerPixel = diff / area;
    fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n", diff, diff / area);
    return diffPerPixel;
}
