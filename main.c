/**
* Threaded bmp image process, apply blur and cheese filter
*
* Completion time: (estimation of hours spent on this program)
*
* @author Connor McCoy
* @version 11/4/23
*/
////////////////////////////////////////////////////////////////////////////////
//INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include "BmpProcessor.h"
////////////////////////////////////////////////////////////////////////////////
//MACRO DEFINITIONS
//problem assumptions
#define BMP_HEADER_SIZE 14
#define BMP_DIB_HEADER_SIZE 40
#define MAXIMUM_IMAGE_SIZE 4096
#define THREAD_COUNT 57
////////////////////////////////////////////////////////////////////////////////
//DATA STRUCTURES
//
struct Circle{
    int x;
    int y;
    int r;
};

struct info{
    int start;//Where threads image array start in relation to global pixel array
    int end;//Where threads image ends in relations to global pixel array
    int height;//Height of threads specific pixel array
    int width;//Width of threads specific pixel array
    struct Pixel** pArr;
    struct Circle* circles; //Only used for cheese filter threads
    int totalCircles;
};

void image_apply_colorshift(struct Pixel** pArr, int height, int width, int rShift, int gShift, int bShift);
void blur(struct Pixel** pArr, struct DIB_Header* header);
void drawCircles(struct Pixel** pArr, struct DIB_Header* header);
void* threaded_blur(void* data);
void* drawThreadedCircles(void* data);
void makeCircles(struct Circle* circles, struct DIB_Header* DIB);
int isIncluded(struct Circle* circles, int x, int y, int size);

////////////////////////////////////////////////////////////////////////////////
//MAIN PROGRAM CODE
void main(int argc, char* argv[]) {
    struct BMP_Header BMP;
    struct DIB_Header DIB;

    //Default file name so program compiles from IDE
    char* file_input_name = "ttt.bmp";
    //Parse file name given through command line

    file_input_name = argv[1];
    //Checking if file entered exists
    if (access(file_input_name, F_OK) != 0) {
        printf("That file doesn't exist, please enter correct file name as first argument\n");
        exit(1);
    }

    //Checking if file type is correct
    if(strcmp(&(file_input_name[strlen(file_input_name)-4]), ".bmp") != 0){
        printf("Input file must be of type .bmp\n");
        exit(1);
    }
    //Open file given from command line
    FILE* file_input = fopen(file_input_name, "rb");

    //Populate header structs
    readBMPHeader(file_input, &BMP);
    readDIBHeader(file_input, &DIB);


    //Allocate memory for pixel array
    struct Pixel** pixels = (struct Pixel**)malloc(sizeof(struct Pixel*) * DIB.height);
    for (int p = 0; p < DIB.height; p++) {
        pixels[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * DIB.width);
    }

    //Read pixel data from bmp image file
    readPixelsBMP(file_input, pixels, DIB.width, DIB.width);
    //Close file, done reading
    fclose(file_input);

    int changedName = 0;

    char* file_output_name;

    int height = DIB.height;
    int width = DIB.width;
    pthread_t tids[THREAD_COUNT];
    pthread_attr_t attr;

    struct info** infos = (struct info**)malloc(sizeof(struct info*) * THREAD_COUNT);

    int choice = 0;

    if(choice == 0){

        int segSize = DIB.width/THREAD_COUNT;

        int remPixels = width - (segSize*THREAD_COUNT);

        int currentPixel = 0;

        //Creating info structs and data splitting for blur threads
        for(int i = 0; i < THREAD_COUNT; i++){

            int padding = 1;

            infos[i] = (struct info*)malloc(sizeof(struct info));

            infos[i]->start = currentPixel;

            infos[i]->height = height;


            if(i == THREAD_COUNT - 1){
                infos[i]-> end = (width - 1);
                infos[i]->width = infos[i]->end - infos[i]->start + 1;
            }else{
                //If there are remaining pixels add to the column size, saving last one for end thread to balance
                if(remPixels > 1){
                    infos[i]-> end = (infos[i]->start + segSize + 1);
                    remPixels--;
                }else{
                    infos[i]-> end = (infos[i]->start + segSize);
                }
                currentPixel = infos[i]->end;

                //Pad the arrays for blur to be accurate
                if(i == 0){
                    infos[i]->end += padding;
                }
                if(i > 0 && i < THREAD_COUNT -1){
                    infos[i]->end += padding;
                    infos[i]->start -= padding;
                }
                if(i == THREAD_COUNT - 1){
                    infos[i]->start -= padding;
                }
                infos[i]->width = infos[i]->end - infos[i]->start;

            }

            struct Pixel** pArr = (struct Pixel**)malloc(sizeof(struct Pixel*)*infos[i]->height);
            for(int p = 0; p < infos[i]->height; p++){
                pArr[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * infos[i]->width);
            }

            infos[i]->pArr = pArr;
            for(int j = 0; j < infos[i]->height; j++){
                for(int k = 0; k< infos[i]->width; k++){
                    infos[i]->pArr[j][k].red = pixels[j][k + infos[i]->start].red;
                    infos[i]->pArr[j][k].blue = pixels[j][k + infos[i]->start].blue;
                    infos[i]->pArr[j][k].green = pixels[j][k + infos[i]->start].green;
                }
            }
            pthread_attr_init(&attr);
            pthread_create(&tids[i],&attr,threaded_blur,infos[i]);
            printf("thread %d created\n", i);
        }

        //Joining threads
        for(int i = 0; i < THREAD_COUNT; i++){
            pthread_join(tids[i],NULL);
            printf("thread %d joined\n", i);
        }

        //Take threaded pArrs and copy their info back into og array
        for(int i = 0; i < THREAD_COUNT; i++){
            printf("Width of threaded array being copied back is %d\n", infos[i]->width);
            printf("Offset with info start is %d \n", infos[i]->start);

            if(i == 0){
                for(int j = 0; j < infos[i]->height; j++){
                    for(int k = 0; k< infos[i]->width - 1; k++){

                        pixels[j][k].red = infos[i]->pArr[j][k].red;
                        pixels[j][k].blue =  infos[i]->pArr[j][k].blue;
                        pixels[j][k].green = infos[i]->pArr[j][k].green;
                    }
                }
            }

            if(i > 0 && i < THREAD_COUNT - 1){
                for(int j = 0; j < infos[i]->height; j++){
                    for(int k = 1; k < infos[i]->width - 1 ; k++){

                        pixels[j][k + infos[i]->start].red = infos[i]->pArr[j][k].red;
                        pixels[j][k + infos[i]->start].blue =  infos[i]->pArr[j][k].blue;
                        pixels[j][k + infos[i]->start].green = infos[i]->pArr[j][k].green;
                    }
                }
            }else{
                for(int j = 0; j < infos[i]->height; j++){
                    for(int k = 1; k< infos[i]->width; k++){

                        pixels[j][k + infos[i]->start].red = infos[i]->pArr[j][k].red;
                        pixels[j][k + infos[i]->start].blue =  infos[i]->pArr[j][k].blue;
                        pixels[j][k + infos[i]->start].green = infos[i]->pArr[j][k].green;
                    }
                }
            }
        }

        //Free all memory that was allocated
        for(int i = 0; i < THREAD_COUNT; i++){
            for(int j = 0; j < infos[i]->height; j++){
                free(infos[i]->pArr[j]);
                infos[i]->pArr[j] = NULL;
            }
            free(infos[i]->pArr);
            infos[i]->pArr = NULL;
            free(infos[i]);
            infos[i]=NULL;
        }
        free(infos);
        infos = NULL;
    }
    else if(choice == 1){
        //Stuff for threaded cheese filter

        //First we need to generate circle data
        int totalN = floor(DIB.width * 0.08);

        struct Circle* circles = (struct Circle*) malloc(sizeof(struct Circle) * totalN);

        makeCircles(circles, &DIB);

        int segSize = DIB.width/THREAD_COUNT;

        int remPixels = width - (segSize*THREAD_COUNT);

        int currentPixel = 0;

        for(int i = 0; i < THREAD_COUNT; i++){

            infos[i] = (struct info*)malloc(sizeof(struct info));
            infos[i]->start = currentPixel;
            infos[i]->height = height;
            if(i == THREAD_COUNT - 1){
                infos[i]-> end = (width - 1);
                infos[i]->width = infos[i]->end - infos[i]->start + 1;
            }else{
                //If there are remaining pixels add to the column size, saving last one for end thread to balance
                if(remPixels > 1){
                    infos[i]-> end = (infos[i]->start + segSize + 1);
                    remPixels--;
                }else{
                    infos[i]-> end = (infos[i]->start + segSize) ;
                }
                currentPixel = infos[i]->end;
                infos[i]->width = infos[i]->end - infos[i]->start;
            }
            struct Pixel** pArr = (struct Pixel**)malloc(sizeof(struct Pixel*)*infos[i]->height);
            for(int p = 0; p < infos[i]->height; p++){
                pArr[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * infos[i]->width);
            }

            infos[i]->pArr = pArr;

            infos[i]->totalCircles = 0;

            for(int j = 0; j < infos[i]->height; j++){
                for(int k = 0; k< infos[i]->width; k++){
                    infos[i]->pArr[j][k].red = pixels[j][k+infos[i]->start].red;
                    infos[i]->pArr[j][k].blue = pixels[j][k+infos[i]->start].blue;
                    infos[i]->pArr[j][k].green = pixels[j][k+infos[i]->start].green;

                    //Populate each thread's circle data
                    for(int l = 0; l < totalN; l++){
                        double circleValue = pow(((k+infos[i]->start) - circles[l].x), 2) + pow(j - circles[l].y, 2);
                        double radSq = pow(circles[l].r,2);
                        int included = isIncluded(infos[i]->circles, circles[l].x, circles[l].y, infos[i]->totalCircles);
                        if(circleValue <= radSq && included == 0){
                            if(infos[i]->totalCircles == 0){
                                struct Circle* threadCircles = (struct Circle*) malloc(sizeof(struct Circle));
                                infos[i]->circles = threadCircles;
                                infos[i]->circles[infos[i]->totalCircles] = circles[l];
                                infos[i]->totalCircles++;
                            }else if(infos[i]->totalCircles > 0){
                                struct Circle* newCircles;
                                infos[i]->totalCircles++;
                                newCircles = (struct Circle*)realloc(infos[i]->circles, sizeof(struct Circle)*infos[i]->totalCircles);
                                infos[i]->circles = newCircles;
                                infos[i]->circles[infos[i]->totalCircles - 1] = circles[l];
                            }
                        }
                    }
                }
            }

            pthread_attr_init(&attr);
            pthread_create(&tids[i],&attr,drawThreadedCircles,infos[i]);
            printf("thread %d created\n", i);
        }

        //Joining threads
        for(int i = 0; i < THREAD_COUNT; i++){
            pthread_join(tids[i],NULL);
            printf("thread %d joined\n", i);
        }

        //Take threaded pArrs and copy their info back into og array
        for(int i = 0; i < THREAD_COUNT; i++){
            printf("Width of threaded array being copied back is %d\n", infos[i]->width);
            printf("Offset with info start is %d \n", infos[i]->start);
            printf("Number of circles in thread %d \n", infos[i]->totalCircles);
            for(int j = 0; j < infos[i]->height; j++){
                for(int k = 0; k< infos[i]->width; k++){
                    pixels[j][k + infos[i]->start].red = infos[i]->pArr[j][k].red;
                    pixels[j][k + infos[i]->start].blue =  infos[i]->pArr[j][k].blue;
                    pixels[j][k + infos[i]->start].green = infos[i]->pArr[j][k].green;
                }
            }
        }


        //Free all memory that was allocated
        for(int i = 0; i < THREAD_COUNT; i++){
            for(int j = 0; j < infos[i]->height; j++){
                free(infos[i]->pArr[j]);
                infos[i]->pArr[j] = NULL;
            }
            free(infos[i]->pArr);
            infos[i]->pArr = NULL;
            free(infos[i]->circles);
            infos[i]->circles = NULL;
            free(infos[i]);
            infos[i]=NULL;
        }

        free(circles);
        circles = NULL;

        free(infos);
        infos = NULL;
    }


    //blur(pixels, &DIB);

    //drawCircles(pixels, &DIB);

    //If output name wasn't given through command line make it default to original name + _copy.bmp
    if(changedName ==0){
        file_input_name[strlen(file_input_name)-4] = '\0';
        file_output_name = strcat(file_input_name, "_copy.bmp");
    }

    //Update the headers
    makeBMPHeader(&BMP, DIB.width, DIB.height);
    makeDIBHeader(&DIB, DIB.width, DIB.height);

    //Open output file for writing
    FILE* file_output = fopen(file_output_name, "wb");

    //Write headers to file
    writeBMPHeader(file_output, &BMP);
    writeDIBHeader(file_output, &DIB);

    //Right pixel array to file
    writePixelsBMP(file_output, pixels, DIB.width,
                   DIB.height);

    //Free memory from pixel array
    for (int p = 0; p < DIB.height; p++) {
        free(pixels[p]);
        pixels[p] = NULL;
    }

    free(pixels);
    pixels = NULL;

    fclose(file_output);
}

//Check if a circle is already included in the array provided
int isIncluded(struct Circle* circles, int x, int y, int size){
    if(size == 0){
        return 0;
    }
    for(int i = 0; i < size; i++){
        if(circles[i].x == x && circles[i].y == y){
            return 1;
        }
    }
    return 0;
}


void makeCircles(struct Circle* circles, struct DIB_Header* DIB){
    int totalN = floor(DIB->width * 0.08);
    int avgR = totalN;
    srand(time(NULL));
    rand();

    //Make first quarter of circles larger
    for(int i = 0; i < totalN / 4; i++){
        int y = rand() % DIB->height;
        int x = rand() % DIB->width;
        circles[i].r = avgR * 1.5;

        //No overlapping, help evenly distribute
        if(i > 0){
            int j = 0;
            while(j < i){
                //Use distance formula to see if distance betwen points is less then sum of each circles radius
                if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                    y = rand() % DIB->height;
                    x = rand() % DIB->width;
                    j = 0; //If there is overlap you have to reset loop so each new center point is compared to every existing circle each time
                } else{
                    j++;
                }
            }
        }

        circles[i].y = y;
        circles[i].x = x;
    }
    //Make second quarter of circles smaller
    for(int i = totalN/4; i < totalN / 2; i++){
        int y = rand() % DIB->height;
        int x = rand() % DIB->width;
        circles[i].r = avgR / 1.5;

        //No overlapping, help evenly distribute
        int j = 0;
        while(j < i){
            if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                y = rand() % DIB->height;
                x = rand() % DIB->width;
                j = 0;
            } else{
                j++;
            }
        }

        circles[i].y = y;
        circles[i].x = x;
    }
    //Make last half of quarter avg sized
    for(int i = totalN/2; i < totalN; i++){
        int y = rand() % DIB->height;
        int x = rand() % DIB->width;
        circles[i].r = avgR;

        //No overlapping, help evenly distribute
        int j = 0;
        while(j < i){
            if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                y = rand() % DIB->height;
                x = rand() % DIB->width;
                j = 0;
            } else{
                j++;
            }
        }
        circles[i].y = y;
        circles[i].x = x;
    }
}

void* drawThreadedCircles(void* data) {
    struct info* info = (struct info*)data;

    for (int i = 0; i < info->height; i++) {
        for (int j = 0; j < info->width; j++) {
            for(int k = 0; k < info->totalCircles; k++){
                double circleValue = pow(((j+info->start) - info->circles[k].x), 2) + pow(i - info->circles[k].y, 2);
                double radSq = pow(info->circles[k].r,2);
                if(circleValue <= radSq){
                    info->pArr[i][j].red = 0;
                    info->pArr[i][j].blue = 0;
                    info->pArr[i][j].green = 0;
                }
            }
        }
    }


    //Make it cheesy
    image_apply_colorshift(info->pArr, info->height, info->width, 56, 56, 0);
}

void* threaded_blur(void* data){

    struct info* info = (struct info*)data;
    int sumRed = 0;
    int sumBlue = 0;
    int sumGreen = 0;
    int validN = 1;

    for (int i = 0; i < info->height; i++) {
        for (int j = 0; j < info->width; j++) {

            validN = 1;
            sumBlue = 0;
            sumGreen = 0;
            sumRed = 0;

            sumBlue += (int)info->pArr[i][j].blue;
            sumGreen += (int)info->pArr[i][j].green;
            sumRed += (int)info->pArr[i][j].red;

            //Bottom Neighbor
            if(i+1 < info->height ){
                sumBlue += (int)info->pArr[i+1][j].blue;
                sumGreen += (int)info->pArr[i+1][j].green;
                sumRed += (int)info->pArr[i+1][j].red;
                validN++;
            }

            //Top Neighbor
            if(i-1 >= 0){
                sumBlue += (int)info->pArr[i-1][j].blue;
                sumGreen += (int)info->pArr[i-1][j].green;
                sumRed += (int)info->pArr[i-1][j].red;
                validN++;
            }

            //Right Neighbor
            if(j+1 < info->width ){
                sumBlue += (int)info->pArr[i][j+1].blue;
                sumGreen += (int)info->pArr[i][j+1].green;
                sumRed += (int)info->pArr[i][j+1].red;
                validN++;
            }
            //Left Neighbor
            if(j-1 >= 0){
                sumBlue += (int)info->pArr[i][j-1].blue;
                sumGreen += (int)info->pArr[i][j-1].green;
                sumRed += (int)info->pArr[i][j-1].red;
                validN++;
            }
            //Bottom Right Neighbor

            if(i + 1 < info->height  && j + 1 < info->width ){
                sumBlue += (int)info->pArr[i+1][j+1].blue;
                sumGreen += (int)info->pArr[i+1][j+1].green;
                sumRed += (int)info->pArr[i+1][j+1].red;
                validN++;
            }

            //Top Right Neighbor
            if(i - 1 >= 0 && j + 1 < info->width ){
                sumBlue += (int)info->pArr[i-1][j+1].blue;
                sumGreen += (int)info->pArr[i-1][j+1].green;
                sumRed += (int)info->pArr[i-1][j+1].red;
                validN++;
            }

            //Bottom Left Neighbor
            if(i + 1 < info->height  && j - 1 >= 0){
                sumBlue += (int)info->pArr[i+1][j-1].blue;
                sumGreen += (int)info->pArr[i+1][j-1].green;
                sumRed += (int)info->pArr[i+1][j-1].red;
                validN++;
            }

            //Top Left Neighbor
            if(i - 1 >= 0 && j - 1 >= 0){
                sumBlue += (int)info->pArr[i-1][j-1].blue;
                sumGreen += (int)info->pArr[i-1][j-1].green;
                sumRed += (int)info->pArr[i-1][j-1].red;
                validN++;
            }

            info->pArr[i][j].blue = (unsigned char)(sumBlue/validN);
            info->pArr[i][j].green = (unsigned char)(sumGreen/validN);
            info->pArr[i][j].red = (unsigned char)(sumRed/validN);

        }
    }

}

//Image color shift from previous Image Processor program
void image_apply_colorshift(struct Pixel** pArr, int height, int width, int rShift, int gShift, int bShift) {
    //Highest value a rgb value can be is 255
    unsigned char highestValue = 255;
    unsigned char lowestValue = 0;
    //Traverse through the pixel array shifting based on given values
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            //If value given is higher or lower than the highest/lowest value clap to the highest/lowest value
            if(pArr[i][j].blue  + bShift > highestValue){
                pArr[i][j].blue = highestValue;
            }
            else if(pArr[i][j].blue + bShift < lowestValue){
                pArr[i][j].blue = lowestValue;
            }
            else{
                //Shift based on given value
                pArr[i][j].blue = pArr[i][j].blue + bShift;
            }
            if(pArr[i][j].red +  rShift > highestValue){
                pArr[i][j].red =  highestValue;
            }
            else if(pArr[i][j].red  + rShift < lowestValue){
                pArr[i][j].red = lowestValue;
            }
            else{
                pArr[i][j].red = pArr[i][j].red +  rShift;
            }
            if(pArr[i][j].green +  gShift > highestValue){
                pArr[i][j].green =  highestValue;
            }
            else if(pArr[i][j].green +  gShift < lowestValue){
                pArr[i][j].green =  lowestValue;
            }
            else{
                pArr[i][j].green = pArr[i][j].green + gShift;}
        }
    }
}