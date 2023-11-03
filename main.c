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
//TODO: finish me


//UNCOMMENT BELOW LINE IF USING SER334 LIBRARY/OBJECT FOR BMP SUPPORT
#include "BmpProcessor.h"
////////////////////////////////////////////////////////////////////////////////

void image_apply_colorshift(struct Pixel** pArr, struct DIB_Header* header, int rShift, int gShift, int bShift);

//MACRO DEFINITIONS
//problem assumptions
#define BMP_HEADER_SIZE 14
#define BMP_DIB_HEADER_SIZE 40
#define MAXIMUM_IMAGE_SIZE 4096
//TODO: finish me
////////////////////////////////////////////////////////////////////////////////
//DATA STRUCTURES
//TODO: finish me
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


    image_apply_colorshift(pixels, &DIB, 56, 0 , 0);


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

void blur(struct Pixel** pArr, struct DIB_Header* header){

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