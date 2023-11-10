/**
* Threaded bmp image process, apply blur and cheese filter
*
* Completion time: 14hrs
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

#define THREAD_COUNT 7
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
    //Variable to check if name is changed in command line or not
    int changedName = 0;

    char* file_output_name;
    //Allocate space for the info structs that will be sent to the threads
    struct info** infos = (struct info**)malloc(sizeof(struct info*) * THREAD_COUNT);

    //Variable to store choice between blur or cheese, default is cheese
    int choice = 1;
    //Starting the arg parser, set to 1 to ignore the ./ProgamName
    int a = 1;
    //Parse through command line arguments//////////////////////////////////////////////////////////////////////////////
    while (argv[a] != NULL){

        //If a valid command is entered valid will be set to 1.
        int valid = 0;

        //Set input file/////////////////////////////////////////////////////////////////////////////////////////////
        if (strcmp(argv[a], "-i") == 0){
            valid = 1;
            a++;
            //Parse file name given through command line

            file_input_name = argv[a];
        }

        //Set filter choice//////////////////////////////////////////////////////////////////////////////////////////////
        if (strcmp(argv[a], "-f") == 0){
            valid = 1;
            a++;
            if (strcmp(argv[a], "b") == 0){
                choice = 0;
            }
            else if (strcmp(argv[a], "c") == 0){
                choice = 1;
            }
        }

        //Update file output name///////////////////////////////////////////////////////////////////////////////////////
        if (strcmp(argv[a], "-o") == 0){
            valid = 1;
            a++;
            file_output_name = argv[a];
            //Making sure output file type is labeled correctly
            if(strcmp(&(file_output_name[strlen(file_output_name)-4]), ".bmp") != 0){
                printf("Output file must be of type .bmp\n");
                exit(1);
            }
            changedName = 1;
        }

        //If an invalid command in entered send message listing off correct commands to user
        if(valid == 0){
            printf("'%s' Is not a valid command!\nEnter:\n-i, follow by valid file input name.\n"
                   "-f followed by b to apply blur filter\n"
                   "-f followed by c to apply cheese filter\n"
                   "-o followed by desired file name to set output file name\n", argv[a]);
            break;
        }
        a++;
    }

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

    //Initialize height and width variables along with tids
    int height = DIB.height;
    int width = DIB.width;
    pthread_t tids[THREAD_COUNT];
    pthread_attr_t attr;

    if(choice == 0){
        //Calculate min segment size based on thread count and width of image
        int segSize = DIB.width/THREAD_COUNT;
        //Find out how many remaining pixels are unaccounted for with just segSize
        int remPixels = width - (segSize*THREAD_COUNT);
        //Set starting pixel variable to 0
        int currentPixel = 0;

        //Creating info structs and data splitting for blur threads
        for(int i = 0; i < THREAD_COUNT; i++){
            //Padding to add to left and or right of each column
            int padding = 1;
            //Allocate the memory for the thread specific info struct
            infos[i] = (struct info*)malloc(sizeof(struct info));
            //Set start equal to current pixel
            infos[i]->start = currentPixel;

            infos[i]->height = height;


            //If there are remaining pixels add to the column size
            if(remPixels > 0){
                infos[i]-> end = (infos[i]->start + segSize + 1);
                remPixels--;
            }else{ //If not just add the seg size to the start inorder to get end position
                infos[i]-> end = (infos[i]->start + segSize);
            }
            //Update the current pixel
            currentPixel = infos[i]->end;
            //Pad the arrays for blur to be accurate
            //Padd only the end if first thread segment
            if(i == 0){
                infos[i]->end += padding;
            }
            //Pad both sides if in the middle
            if(i > 0 && i < THREAD_COUNT -1){
                infos[i]->end += padding;
                infos[i]->start -= padding;
            }
            //Only pad the left side if end segment
            if(i == THREAD_COUNT - 1){
                infos[i]->start -= padding;
            }
            //Set width of each segment for easy traversal
            infos[i]->width = infos[i]->end - infos[i]->start;

            //Allocate the memory for the specific thread based on the width
            struct Pixel** pArr = (struct Pixel**)malloc(sizeof(struct Pixel*)*infos[i]->height);
            for(int p = 0; p < infos[i]->height; p++){
                pArr[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * infos[i]->width);
            }

            infos[i]->pArr = pArr;
            //Populate the threads pixel array based off the global pixel array
            for(int j = 0; j < infos[i]->height; j++){
                for(int k = 0; k< infos[i]->width; k++){
                    infos[i]->pArr[j][k].red = pixels[j][k + infos[i]->start].red;
                    infos[i]->pArr[j][k].blue = pixels[j][k + infos[i]->start].blue;
                    infos[i]->pArr[j][k].green = pixels[j][k + infos[i]->start].green;
                }
            }
            //Create thread and give thread operation the threads data via infos[i]
            pthread_attr_init(&attr);
            pthread_create(&tids[i],&attr,threaded_blur,infos[i]);
        }

        //Joining threads
        for(int i = 0; i < THREAD_COUNT; i++){
            pthread_join(tids[i],NULL);
        }

        //Take threaded pArrs and copy their info back into og array
        for(int i = 0; i < THREAD_COUNT; i++){
            //Account for padding
            //If its the first segment ignore the last column, its just padding
            if(i == 0){
                for(int j = 0; j < infos[i]->height; j++){
                    for(int k = 0; k< infos[i]->width - 1; k++){

                        pixels[j][k].red = infos[i]->pArr[j][k].red;
                        pixels[j][k].blue =  infos[i]->pArr[j][k].blue;
                        pixels[j][k].green = infos[i]->pArr[j][k].green;
                    }
                }
            }
            //If its a middle segment ignore the beginning and end columns, theyre just padding
            if(i > 0 && i < THREAD_COUNT - 1){
                for(int j = 0; j < infos[i]->height; j++){
                    for(int k = 1; k < infos[i]->width - 1 ; k++){

                        pixels[j][k + infos[i]->start].red = infos[i]->pArr[j][k].red;
                        pixels[j][k + infos[i]->start].blue =  infos[i]->pArr[j][k].blue;
                        pixels[j][k + infos[i]->start].green = infos[i]->pArr[j][k].green;
                    }
                }
            }else{
                //add ignore the padding for the last segment
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
        //Creating segments in the same was as was done in blur
        int segSize = DIB.width/THREAD_COUNT;

        int remPixels = width - (segSize*THREAD_COUNT);

        int currentPixel = 0;

        for(int i = 0; i < THREAD_COUNT; i++){
            //Allocate memory for the info threads struct
            infos[i] = (struct info*)malloc(sizeof(struct info));
            infos[i]->start = currentPixel;
            infos[i]->height = height;

            //If there are remaining pixels add to the column size
            if(remPixels > 0){
                infos[i]-> end = (infos[i]->start + segSize + 1);
                remPixels--;
            }else{
                infos[i]-> end = (infos[i]->start + segSize) ;
            }
            currentPixel = infos[i]->end;
            infos[i]->width = infos[i]->end - infos[i]->start;
            //Allocating the array memory for the threads
            struct Pixel** pArr = (struct Pixel**)malloc(sizeof(struct Pixel*)*infos[i]->height);
            for(int p = 0; p < infos[i]->height; p++){
                pArr[p] = (struct Pixel*)malloc(sizeof(struct Pixel) * infos[i]->width);
            }

            infos[i]->pArr = pArr;
            //Set the initial value of the threads total number of circles it needs to draw to 0
            infos[i]->totalCircles = 0;
            //Populate the threads pixel array based of corresponding pixels in global array as before
            for(int j = 0; j < infos[i]->height; j++){
                for(int k = 0; k< infos[i]->width; k++){
                    infos[i]->pArr[j][k].red = pixels[j][k+infos[i]->start].red;
                    infos[i]->pArr[j][k].blue = pixels[j][k+infos[i]->start].blue;
                    infos[i]->pArr[j][k].green = pixels[j][k+infos[i]->start].green;

                    //Populate each thread's circle data
                    for(int l = 0; l < totalN; l++){
                        //Calculate if the pixel is one that will need to be drawn into the circle
                        double circleValue = pow(((k+infos[i]->start) - circles[l].x), 2) + pow(j - circles[l].y, 2);
                        double radSq = pow(circles[l].r,2);
                        //Run isIncluded helper function so that we don't add circles that are already added
                        int included = isIncluded(infos[i]->circles, circles[l].x, circles[l].y, infos[i]->totalCircles);
                        if(circleValue <= radSq && included == 0){
                            if(infos[i]->totalCircles == 0){
                                //If there is no circles added yet we allocate memory and add the circle
                                struct Circle* threadCircles = (struct Circle*) malloc(sizeof(struct Circle));
                                infos[i]->circles = threadCircles;
                                infos[i]->circles[infos[i]->totalCircles] = circles[l];
                                infos[i]->totalCircles++;
                            }else if(infos[i]->totalCircles > 0){
                                struct Circle* newCircles;
                                infos[i]->totalCircles++;
                                //Use realloc to resize the array to fit a new circle struct
                                newCircles = (struct Circle*)realloc(infos[i]->circles, sizeof(struct Circle)*infos[i]->totalCircles);
                                infos[i]->circles = newCircles;
                                infos[i]->circles[infos[i]->totalCircles - 1] = circles[l];
                            }
                        }
                    }
                }
            }
            //Create the threads and send the info to the runner function
            pthread_attr_init(&attr);
            pthread_create(&tids[i],&attr,drawThreadedCircles,infos[i]);
        }

        //Joining threads
        for(int i = 0; i < THREAD_COUNT; i++){
            pthread_join(tids[i],NULL);
        }

        //Take threaded pArrs and copy their info back into og array
        for(int i = 0; i < THREAD_COUNT; i++){
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
                //Use distance formula to see if distance between points is less then sum of each circles radius
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
                //Using the formula for a circle to determine if the pixel falls inside the circle then turn that pixel black
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
    //Sum totals of each neighboring r/b/g value
    int sumRed = 0;
    int sumBlue = 0;
    int sumGreen = 0;
    //Sum of valid neighbors to sum and properly average, always will start with 1
    int validN = 1;

    for (int i = 0; i < info->height; i++) {
        for (int j = 0; j < info->width; j++) {

            validN = 1;
            sumBlue = 0;
            sumGreen = 0;
            sumRed = 0;
            //Update sums with the currently selected pixels values, will always be valid
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
            //Calculate the average value of each r/b/g pixel and assign it to current pixel's rbg values
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