#include <stdio.h>
#include <stdlib.h>
#include <time.h>
// Data
#define MAXARRAY 200
// Function prototype
void mergeSort(int[], int, int);
// Main program
int main(void)
{
int data[MAXARRAY];
int i = 0;
// Load random data into the array
// Note: srand() function is not used here.
// Hence, random number generated will be same every time the application is executed.
// This makes it easier to view the sorted results.
for(i = 0; i < MAXARRAY; i++)
{
data[i] = rand() % 100;
}
// Print data before sorting
printf("Before Sorting:\n");
for(i = 0; i < MAXARRAY; i++)
{
printf(" %d", data[i]);
}
printf("\n");
// Call the merge sort function
mergeSort(data, 0, MAXARRAY - 1);
// Print data after sorting
printf("\n");
printf("After sorting using Mergesort:\n");
for(i = 0; i < MAXARRAY; i++)
{
printf(" %d", data[i]);
}
printf("\n");
return 0;
}
// Function definition
void mergeSort(int inputData[], int startPoint, int endPoint)
{
int i = 0;
int length = endPoint - startPoint + 1;
int pivot = 0;
int merge1 = 0;
int merge2 = 0;
int working[MAXARRAY] = {0};
if(startPoint == endPoint)
{
return;
}
pivot = (startPoint + endPoint) / 2;
// Recursive function call
mergeSort(inputData, startPoint, pivot);
mergeSort(inputData, pivot + 1, endPoint);
for(i = 0; i < length; i++)
{
working[i] = inputData[startPoint + i];
}
merge1 = 0;
merge2 = pivot - startPoint + 1;
for(i = 0; i < length; i++)
{
if(merge2 <= endPoint - startPoint)
if(merge1 <= pivot - startPoint)
if(working[merge1] > working[merge2])
{
inputData[i + startPoint] =
working[merge2++];
}
else
{
inputData[i + startPoint] =
working[merge1++];
}
else
{
inputData[i + startPoint] = working[merge2++];
}
else
{
inputData[i + startPoint] = working[merge1++];
}
}
}
