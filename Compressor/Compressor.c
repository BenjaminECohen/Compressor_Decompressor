#define _CRT_SECURE_NO_WARNINGS
#define WRITE_SIZE (16)

// Compressor.c : This file contains the 'main' function. Program execution begins and ends there.
//X Y Z U V TEXTURE NAME R G B


#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

int integerSize = 0;

struct Vertices
{
    float x;
    float y;
    float z;
    char textureName[32];
}Vertices;

float CheckMin(float value, float min)
{
    if (value < min)
    {
        return value;
    }
    return min;
}

float CheckMax(float value, float max)
{
    if (value > max)
    {
        return value;
    }
    return max;
}



uint16_t GetPackedDecimal(float value, int depth, int maxDepth, uint16_t* result)
{
    float fixedPoint = 1.0f / pow(2, depth);

    if (value >= fixedPoint) //Greater than amounts 0.5, 0.25, 0.125, etc.
    {
        *result |= 1; //Set the bit

        if (depth != maxDepth) //Did we traverse enough?
        {
            *result <<= 1;
            *result |= GetPackedDecimal(value - fixedPoint, ++depth, maxDepth, result);
            return *result;
        }
        
        return *result;

    }
    else
    {
        if (depth != maxDepth) //Did we traverse enough?
        {
            *result <<= 1;
            *result |= GetPackedDecimal(value, ++depth, maxDepth, result);
            return *result;
        }

        return *result;
    }
}

uint16_t PackDecimal(float value, int maxDepth)
{
    uint16_t result = 0;
    int depth = 1;
    if (value == 0)
    {
        return result;
    }
    if (maxDepth == 0)
    {
        return result;
    }
    else
    {
        return GetPackedDecimal(value, depth, maxDepth, &result);
    }

}

uint16_t CompressFloat(float value, char compression)
{
    int decimalBits = compression - integerSize;
    //---
    //PACK INTEGER VALUE
    //---
    uint16_t intVal = value;
    //printf("Integer val = %d ", intVal);

    //---
    //PACK EXPONENT
    //---
    if (decimalBits != 0) //If not equal, we have room to pack decimals
    {
        uint16_t deciVal = PackDecimal(value - intVal, decimalBits);

        //printf("Fixed Deci Val %d ", deciVal);
        intVal <<= (decimalBits);
        return (intVal | deciVal); //OR TO PACK
        
    }

}

void PackAndSend(uint16_t compressedVal, char compression, uint16_t * writeWord, char * writeBitCount, FILE* stream)
{
    //Pack into words
    if (compression + *writeBitCount <= WRITE_SIZE) //We have room :3
    {

        *writeWord <<= compression;      //Shift Bits By compression Size
        *writeWord |= compressedVal;     //OR the compressed Val into the writeWord
        *writeBitCount += compression;   //Increase the amount of bits that have been writtern
        //printf("Write word %d\n", *writeWord);


    }
    else //Don't have room
    {
        //---
        //Pack in the rest of the right word
        //---

        //printf("Not enough room in write Word, time to SPLIT!!! %d bits left\n");
        int freeSpace = WRITE_SIZE - *writeBitCount;     //Determine how much freespace we have
        *writeWord <<= freeSpace;                    //Shift left by amount of free space

        uint16_t temp = compressedVal;              //Save to temporary var
        temp >>= (compression - freeSpace);         //Shift temp to the right by the compression - freeSpace so MSB are in lower freespace
        *writeWord |= temp;                          //Get bits into writeWord to fill

        //PRINT WRITE WORD TO OUTPUT FILE!!!!!!!!!!!
        //printf("Write word %d\n", *writeWord);
        fwrite(writeWord, sizeof(uint16_t), 1, stream);

        //---
        //Trim compressed Val of prev packed bits and OR to NEW writeWord
        //---
        *writeWord = 0;
        *writeBitCount = compression - freeSpace;                 //New bit count will be the size minus (compression minus what has already been taken)

        compressedVal <<= WRITE_SIZE - *writeBitCount;            //Get rid of prev packed bits
        compressedVal >>= WRITE_SIZE - *writeBitCount;            //Make sure to set to lower half
        *writeWord |= compressedVal;                 //Put what is left into the writeWord

        //printf("NEW write word %d with %d bits taken already\n", *writeWord, compression - freeSpace);

    }

    if (*writeBitCount == 16)
    {
        //WRITE TO FILE
        //printf("WRITE WORD IS EXACTLY AT 16, WRITE TO FILE!\n");
        //printf("Write word %d\n", *writeWord);
        fwrite(writeWord, sizeof(uint16_t), 1, stream);
        *writeBitCount = 0;
        *writeWord = 0;
    }
}

int FindLeastBitsNeeded(FILE* stream, FILE* statStream)
{

    float max = 0;
    float min = 100;

    while (1)
    {
        float x;
        float y;
        float z; 

        int temp = fscanf(stream, "%*s %f %f %f %*f %*f %*s %*f %*f %*f", &x, &y, &z);

        max =CheckMax(x, max);
        max = CheckMax(y, max);
        max = CheckMax(z, max);

        min = CheckMin(x, min);
        min = CheckMin(y, min);
        min = CheckMin(z, min);

        if (feof(stream))
        {
            break;
        }

    }
    
    fprintf(statStream, "Max value in data is %f\n", max);
    fprintf(statStream, "Min value in data is %f\n", min);

    int maxInteger = (int)sqrt(ceil((double)max));

    fprintf(statStream, "Bits dedicated to fit max value of %f is %d bits which holds a max value of %d\n", max, maxInteger, (int)pow(2, maxInteger) - 1);

    if (min < 0)
    {
        fprintf(statStream, "Bits dedicated to fit sign %d\n", 1);
        //We have negative values +1 to max needed value
        maxInteger++;
    }
    else
    {
        fprintf(statStream, "Bits dedicated to fit sign %d\n", 0);
    }

    

    return maxInteger;
}




int main(int argc, char *argv[])
{
    char compression = 5;

    if (argc >= 2)
    {
        compression = atoi(argv[1]);
    }
    else
    {
        compression = 5;
    }

    if (compression < 5 || compression > 16)
    {
        printf("Invalid Compression Size Given, defaulting to compression 5\n");
        compression = 5;
    }
   
    //printf("Compression: %d bits\n", compression);

    int maxCompressionVal = pow(2, compression) - 1;

    
    uint8_t hCompression = compression;
    uint16_t size = 0;

    FILE* vertFile = fopen("verts.txt", "r");
    char vertNum[255];

    struct Vertices vert;
    
    char fileBuffer[32];
    sprintf(fileBuffer, "output_%d.bin", compression);

    char statsFileBuffer[32];
    sprintf(statsFileBuffer, "stats_%d.txt", compression);

    FILE* outputStream = fopen(fileBuffer, "wb"); //Output Stream
    uint16_t writeWord = 0;
    char writeBitCount = 0; //How many bits have been written to the Write Byte
    
    FILE* statsStream = fopen(statsFileBuffer, "w");
    
    fprintf(statsStream, "Compression Size: %d bits per value.\n", compression);
    fprintf(statsStream, "Data is written to bitstream of %d bits / %hu bytes\n", WRITE_SIZE, sizeof(uint16_t));

    fpos_t savePos;
    fgetpos(vertFile, &savePos);

    integerSize = FindLeastBitsNeeded(vertFile, statsStream);

    fprintf(statsStream, "Bits used for value past decimal %d\n", (int)compression - integerSize);

    fsetpos(vertFile, &savePos);


    //Write the header of the file
    fwrite(&compression, sizeof(char), 1, outputStream);
    fwrite(&integerSize, sizeof(char), 1, outputStream);

    fpos_t sizePos;
    fgetpos(outputStream, &sizePos);
    fwrite(&size, sizeof(uint16_t), 1, outputStream);

    while (1)
    {
        fscanf(vertFile, "%s %f %f %f %*f %*f %s %*f %*f %*f", vertNum, &vert.x, &vert.y, &vert.z, &vert.textureName);
        //printf("%s %f %f %f %f %f %s %f %f %f\n", vertNum, vert.x, vert.y, vert.z, vert.u, vert.v, vert.textureName, vert.r, vert.g, vert.b);

        
        int length = strlen(vertNum);
        char temp[10];
        memset(temp, '\0', 10);
        strncpy(temp, vertNum, length - 1);
        uint16_t numVerts = (uint16_t)atoi(temp);
        //printf("Section %d\n", numVerts);

        //X VAL
        uint16_t compressedVal = CompressFloat(vert.x, compression);
        //printf("Compressing X val: %f %d\n", vert.x, compressedVal);
        PackAndSend(compressedVal, compression, &writeWord, &writeBitCount, outputStream);


        //Y VAL
        compressedVal = CompressFloat(vert.y, compression);
        //printf("Compressing Y val: %f %d\n", vert.y, compressedVal);
        PackAndSend(compressedVal, compression, &writeWord, &writeBitCount, outputStream);


        //Z VAL
        compressedVal = CompressFloat(vert.z, compression);
        //printf("Compressing Z val: %f %d\n", vert.z, compressedVal);
        PackAndSend(compressedVal, compression, &writeWord, &writeBitCount, outputStream);
        

        if (feof(vertFile))
        {
            int freeSpace = WRITE_SIZE - writeBitCount;             //Get empty space left
            fprintf(statsStream, "%d bits of unusued, zero'd space at the end of the output file\n", freeSpace);
            //printf("FREE SPACE AT END OF FILE %d\n", freeSpace);
            writeWord <<= freeSpace;                                //Shift All bits so MSB is on most significant place
            //PRINT TO FILE                                         //PRINT TO THE FILE BABY!!!!!!!!
            fwrite(&writeWord, sizeof(uint16_t), 1, outputStream);
            break;
        }
        
    }

    //Print the size of the total file to the header
    int length = strlen(vertNum);
    char temp[10];
    memset(temp, '\0', 10);
    strncpy(temp, vertNum, length - 1);
    uint16_t numVerts = (uint16_t)atoi(temp);
    numVerts += 1;
    fsetpos(outputStream, &sizePos);
    fwrite(&numVerts, sizeof(uint16_t), 1, outputStream);

 
    
    fclose(statsStream);
    fclose(vertFile);
    fclose(outputStream);


}

