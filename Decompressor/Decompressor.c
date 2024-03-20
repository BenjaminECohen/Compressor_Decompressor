#define _CRT_SECURE_NO_WARNINGS
#define DATA_PARSE_SIZE (16)


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
	char textureName[31];
}Vertices;

float UnpackDecimal(uint16_t value, int maxDepth)
{
	if (maxDepth == 0)
	{
		return 0;
	}

	float fixedPoint = 1.0f / pow(2, maxDepth);

	if (value & 1) //is the rightmost bit set?
	{
		value >>= 1;												//Shift value 1 bit to the right
		return fixedPoint + UnpackDecimal(value, --maxDepth);	    //Return fixed point + NEXT LEVEL
	}
	else
	{
		value >>= 1;
		return 0 + UnpackDecimal(value, --maxDepth);
	}
	
}

float UnpackValue(uint16_t value, int compression)
{
	float unpackedValue = 0;
	uint16_t temp = value;

	int decimalDepth = compression - integerSize;

	//GET INTEGER VALUE
	temp >>= decimalDepth;				//Get rid of decimals from temp to get just integer value
	unpackedValue += (float)temp;

	//GET DECIMAL VALUE
	uint16_t bitmask = (1 << decimalDepth) - 1;
	uint16_t tempVal = value & bitmask;
	unpackedValue += UnpackDecimal(tempVal, decimalDepth);

	return unpackedValue;

}

void DifCheck(float val1, float val2, float range)
{
	if (abs(val1 - val2) > range)
	{
		//printf("WEE WOOOOO!!!\n");
	}
}


double GetSumOfSquaresFromData(char compression, int sizeOfData, FILE* stream, FILE* vertStream)
{
	float result = 0;
	uint16_t bitmask = (1 << compression) - 1;
	uint16_t currentWord = 0;
	fread(&currentWord, sizeof(uint16_t), 1, stream);

	uint16_t manipulatedWord = 0;
	float value = 0;

	int bitsLeft = DATA_PARSE_SIZE;

	//Vertex File data
	struct Vertices vert;
	int counter = 0;
	char tupleNum[255];

	int  fillAmt = fscanf(vertStream, "%*s %f %f %f %*f %*f %s %*f %*f %*f", &vert.x, &vert.y, &vert.z, &vert.textureName);

	for (int i = 0; i < sizeOfData; i++)
	{
		manipulatedWord = currentWord;

		//Do we need next vert data?

		if (bitsLeft >= compression) //We still have a full value in the current word
		{
			bitsLeft -= compression;
			manipulatedWord >>= bitsLeft;
			manipulatedWord &= bitmask;
			//printf("Manipulated Word: %d\n", manipulatedWord);
			value = UnpackValue(manipulatedWord, compression);
			//printf("Resulting float: %f\n", value);

			if (bitsLeft == 0)		//Get new word if this is the case
			{
				fread(&currentWord, sizeof(uint16_t), 1, stream);
				//printf("New Current Word: %hu at %p\n", currentWord, *stream);
				bitsLeft = DATA_PARSE_SIZE;
			}
	
		}
		else			//Not enough bits left, time to merge
		{
			int dynamicMask = (1 << bitsLeft) - 1;					//Mask out the bits that aren't part of this incomplete value
			manipulatedWord &= dynamicMask;

			//IMPORTANT
			fread(&currentWord, sizeof(uint16_t), 1, stream);		//Get new current word
			//printf("New Current Word: %hu at %p\n", currentWord, *stream);

			int bitsNeeded = compression - bitsLeft;				//How many bits are missing
			manipulatedWord <<= bitsNeeded;							//Shift to OR remaining bits

			uint16_t temp = currentWord;							//Save to temp for manipulation
			temp >>= (DATA_PARSE_SIZE - bitsNeeded);				//shift right to keep just the bits we need
			manipulatedWord |= temp;								//OR the bits to get the full value

			//printf("Manipulated Word: %d\n", manipulatedWord);
			value = UnpackValue(manipulatedWord, compression);
			//printf("Resulting float: %f\n", value);

			bitsLeft = DATA_PARSE_SIZE - bitsNeeded;				//Get the new amount of bits left after using the bits needed
		}

		switch (counter)
		{
			case 0:
				//Case this is an X value
				result += pow(vert.x - value, 2);
				DifCheck(vert.x, value, 1.0f / pow(2, compression - integerSize));
				counter++;
				//printf("X: Actual - predicted: %f %f\n", vert.x, value);
				break;

			case 1:
				//Case for Y value
				result += pow(vert.y - value, 2);
				DifCheck(vert.y, value, 1.0f / pow(2, compression - integerSize));
				counter++;
				//printf("Y: Actual - predicted: %f %f\n", vert.y, value);
				break;

			case 2:
				//Case for Z value
				result += pow(vert.z - value, 2);
				DifCheck(vert.z, value, 1.0f / pow(2, compression - integerSize));
				//printf("Z: Actual - predicted: %f %f\n\n", vert.z, value);
				//Reset counter and get next tuple
				counter = 0;
				fillAmt = fscanf(vertStream, "%*s %f %f %f %*f %*f %s %*f %*f %*f", &vert.x, &vert.y, &vert.z, &vert.textureName);
				break;

			default:
				printf("WE SHOULDN'T BE HERE?!?!?!");
					break;
		}

	}

	return result;
}


int main(int argc, char* argv[])
{
	char* fileToOpen = argv[1];

	//printf("File to open %s\n", fileToOpen);

	FILE* inputStream = fopen(fileToOpen, "rb");
	FILE* vertStream = fopen("verts.txt", "r");
	char compression = 0;
	uint16_t numTuples = 0;
	
	double sumOfSquares = 0;
	const double TUPLE = 3.0;


	fread(&compression, sizeof(char), 1, inputStream);
	//printf("%d\n", compression);
	fread(&integerSize, sizeof(char), 1, inputStream);
	fread(&numTuples, sizeof(uint16_t), 1, inputStream);
	//printf("%d\n", numTuples);

	sumOfSquares = GetSumOfSquaresFromData(compression, numTuples * TUPLE, inputStream, vertStream);

	

	//COMPUTE RMS
	double RMS = sqrt(sumOfSquares / (numTuples * TUPLE));
	printf("RMS: %Lf", RMS);
	//printf("Compression: %d bits\nRMS: %Lf", compression, RMS);

	fclose(inputStream);
	fclose(vertStream);



	


}