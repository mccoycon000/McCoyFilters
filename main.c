/**
* (basic description of the program or class)
*
* Completion time: (estimation of hours spent on this program)
*
* @author (your name), (anyone else, e.g., Acuna, whose code you used)
* @version (a version number or a date)
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


//UNCOMMENT BELOW LINE IF USING SER334 LIBRARY/OBJECT FOR BMP SUPPORT
#include "BmpProcessor.h"
////////////////////////////////////////////////////////////////////////////////

void image_apply_colorshift(struct Pixel** pArr, struct DIB_Header* header, int rShift, int gShift, int bShift);
void blur(struct Pixel** pArr, struct DIB_Header* header);
void drawCircles(struct Pixel** pArr, struct DIB_Header* header);
void* threaded_blur(void* data);
//MACRO DEFINITIONS
//problem assumptions
#define BMP_HEADER_SIZE 14
#define BMP_DIB_HEADER_SIZE 40
#define MAXIMUM_IMAGE_SIZE 4096
#define THREAD_COUNT 4
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
};
////////////////////////////////////////////////////////////////////////////////
//MAIN PROGRAM CODE
//TODO: finish me
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

    int padding = (3-((width/THREAD_COUNT)%3))%3;

    struct info** infos = (struct info**)malloc(sizeof(struct info*) * THREAD_COUNT);

    //Creating info structs and data splitting for blur threads
    for(int i = 0; i < THREAD_COUNT; i++){
        infos[i] = (struct info*)malloc(sizeof(struct info));
        infos[i]->start = i*(width/THREAD_COUNT);
        infos[i]-> end = (width/(THREAD_COUNT+i)) + padding;
        infos[i]->height = height;
        infos[i]->width = (width/THREAD_COUNT) + padding;
        struct Pixel** pArr = (struct Pixel**)malloc(sizeof(struct Pixel*)*infos[i]->height);
        for(int p = 0; p < infos[i]->height; p++){
            pArr[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * infos[i]->width);
            printf("Allocating each threads memory p = %d, thread = %d\n", p, i);
        }
        infos[i]->pArr = pArr;
        for(int j = 0; j < infos[i]->height; j++){
            for(int k = 0; k< infos[i]->width; k++){
                infos[i]->pArr[j][k].red = pixels[j][k+infos[i]->start].red;
                infos[i]->pArr[j][k].blue = pixels[j][k+infos[i]->start].blue;
                infos[i]->pArr[j][k].green = pixels[j][k+infos[i]->start].green;
                printf("Copying from big array to thread array, j = %d, k = %d, thread = %d\n", j, k, i);
            }
        }
        pthread_create(&tids[i],NULL,threaded_blur,infos[i]);
    }

    for(int i = 0; i < THREAD_COUNT; i++){
        pthread_join(tids[i],NULL);
    }

    for(int i = 0; i < THREAD_COUNT; i++){
        for(int j = 0; j < infos[i]->height; j++){
            for(int k = 0; k< infos[i]->width - padding; k++){
                pixels[j][k + infos[i]->start].red = infos[i]->pArr[j][k].red;
                pixels[j][k + infos[i]->start].blue =  infos[i]->pArr[j][k].blue;
                pixels[j][k + infos[i]->start].green = infos[i]->pArr[j][k].green;
                printf("Copying from threaded arrays to big array, j = %d, k = %d, thread = %d\n", j, k, i);
            }
        }
    }

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


void drawCircles(struct Pixel** pArr, struct DIB_Header* header) {

    //First we need to generate circle data
    int totalN = floor(header->width * 0.08);
    int avgR = totalN;
    srand(time(NULL));
    rand();


   struct Circle* circles = (struct Circle*) malloc(sizeof(struct Circle) * totalN);

    //Make first quarter of circles larger
    for(int i = 0; i < totalN / 4; i++){
        int y = rand() % header->height;;
        int x = rand() % header->width;
        circles[i].r = avgR * 1.5;

        //No overlapping, help evenly distribute
        if(i > 0){
            int j = 0;
            while(j < i){
                //Use distance formula to see if distance betwen points is less then sum of each circles radius
                if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                    y = rand() % header->height;;
                    x = rand() % header->width;
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
        int y = rand() % header->height;
        int x = rand() % header->width;
        circles[i].r = avgR / 1.5;

        //No overlapping, help evenly distribute
        int j = 0;
        while(j < i){
            if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                y = rand() % header->height;;
                x = rand() % header->width;
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
        int y = rand() % header->height;
        int x = rand() % header->width;
        circles[i].r = avgR;

        //No overlapping, help evenly distribute
        int j = 0;
        while(j < i){
            if((sqrt(pow(x - circles[j].x, 2) + pow(y - circles[j].y, 2))) <= (circles[i].r + circles[j].r)){
                y = rand() % header->height;;
                x = rand() % header->width;
                j = 0;
            } else{
                j++;
            }
        }
    circles[i].y = y;
    circles[i].x = x;
    }

    for (int i = 0; i < header->height; i++) {
        for (int j = 0; j < header->width; j++) {
            for(int k = 0; k < totalN; k++){
                double circleValue = pow((j - circles[k].x), 2) + pow(i - circles[k].y, 2);
                double radSq = pow(circles[k].r,2);
                if(circleValue <= radSq){
                    pArr[i][j].red = 0;
                    pArr[i][j].blue = 0;
                    pArr[i][j].green = 0;
                }
            }
        }
    }

    //Make it cheesy
    image_apply_colorshift(pArr, header, 56, 56, 0);

    free(circles);
    circles = NULL;
}
void blur(struct Pixel** pArr, struct DIB_Header* header){
    int sumRed = 0;
    int sumBlue = 0;
    int sumGreen = 0;
    int validN = 1;

    for (int i = 0; i < header->height; i++) {
        for (int j = 0; j < header->width; j++) {

            validN = 1;
            sumBlue = 0;
            sumGreen = 0;
            sumRed = 0;

            sumBlue += (int)pArr[i][j].blue;
            sumGreen += (int)pArr[i][j].green;
            sumRed += (int)pArr[i][j].red;

            //Bottom Neighbor
            if(i+1 < header->height ){
                sumBlue += (int)pArr[i+1][j].blue;
                sumGreen += (int)pArr[i+1][j].green;
                sumRed += (int)pArr[i+1][j].red;
                validN++;
            }

            //Top Neighbor
            if(i-1 >= 0){
                sumBlue += (int)pArr[i-1][j].blue;
                sumGreen += (int)pArr[i-1][j].green;
                sumRed += (int)pArr[i-1][j].red;
                validN++;
            }

            //Right Neighbor
            if(j+1 < header->width ){
                sumBlue += (int)pArr[i][j+1].blue;
                sumGreen += (int)pArr[i][j+1].green;
                sumRed += (int)pArr[i][j+1].red;
                validN++;
            }
            //Left Neighbor
            if(j-1 >= 0){
                sumBlue += (int)pArr[i][j-1].blue;
                sumGreen += (int)pArr[i][j-1].green;
                sumRed += (int)pArr[i][j-1].red;
                validN++;
            }
            //Bottom Right Neighbor

            if(i + 1 < header->height  && j + 1 < header->width ){
                sumBlue += (int)pArr[i+1][j+1].blue;
                sumGreen += (int)pArr[i+1][j+1].green;
                sumRed += (int)pArr[i+1][j+1].red;
                validN++;
            }

            //Top Right Neighbor
            if(i - 1 >= 0 && j + 1 < header->width ){
                sumBlue += (int)pArr[i-1][j+1].blue;
                sumGreen += (int)pArr[i-1][j+1].green;
                sumRed += (int)pArr[i-1][j+1].red;
                validN++;
            }

            //Bottom Left Neighbor
            if(i + 1 < header->height  && j - 1 >= 0){
                sumBlue += (int)pArr[i+1][j-1].blue;
                sumGreen += (int)pArr[i+1][j-1].green;
                sumRed += (int)pArr[i+1][j-1].red;
                validN++;
            }

            //Top Left Neighbor
            if(i - 1 >= 0 && j - 1 >= 0){
                sumBlue += (int)pArr[i-1][j-1].blue;
                sumGreen += (int)pArr[i-1][j-1].green;
                sumRed += (int)pArr[i-1][j-1].red;
                validN++;
            }

            pArr[i][j].blue = (unsigned char)(sumBlue/validN);
            pArr[i][j].green = (unsigned char)(sumGreen/validN);
            pArr[i][j].red = (unsigned char)(sumRed/validN);
        }
    }

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

            printf("Threaded blurr, i = %d, j = %d, thread = %d\n", i, j, i);
        }
    }

}

//Image color shift from previous Image Processor program
void image_apply_colorshift(struct Pixel** pArr, struct DIB_Header* header, int rShift, int gShift, int bShift) {
    //Highest value a rgb value can be is 255
    unsigned char highestValue = 255;
    unsigned char lowestValue = 0;
    //Traverse through the pixel array shifting based on given values
    for (int i = 0; i < header->height; i++) {
        for (int j = 0; j < header->width; j++) {
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