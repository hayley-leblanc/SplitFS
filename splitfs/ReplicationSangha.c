#include <stdio.h>
#include <string.h>
#pragma warning(disable : 4996)
#define MAX 256

void make_copy(FILE* fp1, FILE* fp2, FILE* fp3)
{
    char ch;

    while ((ch = fgetc(fp1)) != EOF)
    {
        fputc(ch, fp2);
        fputc(ch, fp3);
    }
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
}

void make_replicas(FILE* fp1, char filepath2[], char filepath3[])
{
    FILE* fp2, * fp3;

    fp2 = fopen(filepath2, "wb");

    if (!fp2) {
        printf("Unable to open the output file.\n");
    }

    fp3 = fopen(filepath3, "wb");

    if (!fp3) {
        printf("Unable to open the output file.\n");
    }

    make_copy(fp1, fp2, fp3);
}
void replication(char filepath1[], char filepath2[], char filepath3[])
{
    FILE* fp1;

    fp1 = fopen(filepath1, "r");

    if (!fp1)
    {
        printf("Unable to open the input file.\n");
        return 0;
    }

    make_replicas(fp1, filepath2, filepath3);
}


int *retrieve_replicas(char filepath1[], char filepath2[], char filepath3[])
{
    FILE* fp1, * fp2, * fp3;
    int *fparray[3];
    fp1 = fopen(filepath1, "r");
    fp2 = fopen(filepath2, "r");
    fp3 = fopen(filepath3, "r");
    fparray[0] = fp1;
    fparray[1] = fp2;
    fparray[2] = fp3;
    return fparray;

}

int delete_replica(char filepath1[])
{
    return remove(filepath1);
}
