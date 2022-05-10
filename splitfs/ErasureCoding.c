#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_FILENAME_LEN 256

int splitFileContents(char main_file_path[],char first_file_path[], char second_file_path[]);
int combineFileContents(char first_file_path[], char second_file_path[]);
int CreateParityP(char first_file_path[], char second_file_path[], char P_file_path[]);
int CreateParityQ(char first_file_path[], char second_file_path[], char Q_file_path[]);
int decode_one_fault();
int decode_when_fileA_fileP_are_lost();
int decode_when_fileB_fileP_are_lost();
int decode_when_fileA_fileB_are_lost();
int XOR_Decode(int x, int y);
int XOR_Encode(int x, int y);

int main()   
{   
    char main_file_path[MAX_FILENAME_LEN] = "/Users/saamajanaraharisetti/ErasureCoding1.txt";
    char first_file_path[MAX_FILENAME_LEN] = "/Users/saamajanaraharisetti/ErasureCoding3.txt";
    char second_file_path[MAX_FILENAME_LEN] = "/Users/saamajanaraharisetti/ErasureCoding4.txt";
    char P_file_path[MAX_FILENAME_LEN] = "/Users/saamajanaraharisetti/ErasureCodingPlease.txt";
    char Q_file_path[MAX_FILENAME_LEN] = "/Users/saamajanaraharisetti/ErasureCodingParityQ.txt";
    //splitFileContents(main_file_path,first_file_path,second_file_path);
    //combineFileContents(first_file_path,second_file_path);
    //CreateParityP(first_file_path,second_file_path,P_file_path);
    //CreateParityQ(first_file_path,second_file_path,Q_file_path);

    //decode_one_fault();
    //decode_when_fileA_fileP_are_lost();
    //decode_when_fileB_fileP_are_lost();
    //decode_when_fileA_fileB_are_lost();
    return 0;
}  
int splitFileContents(char main_file_path[],char first_file_path[], char second_file_path[])
{
    int n;
    int mid_num;
    FILE* fp1, * fp2, *fp3;
    char c;
    fp1 = fopen(main_file_path, "r");
    fp2 = fopen(first_file_path, "wb");
    fp3 = fopen(second_file_path, "wb");
    if (fp1 == NULL) {
        printf("File Not Found!\n");
        return -1;
    }
    if (fp2 == NULL) {
        printf("File Not Found!\n");
        return -1;
    }
    if (fp3 == NULL) {
        printf("File Not Found!\n");
        return -1;
    }

    // calculating the size of the file
    fseek(fp1, 0L, SEEK_END);
    long int total_file_size = ftell(fp1);
    fseek(fp1, 0L, SEEK_SET);

    //Finding the middle of the file so that we can divide into two
    if(total_file_size%2==0)
    mid_num=(total_file_size/2)-1;
    else
    mid_num=total_file_size/2;
     
   int counter = mid_num;
   while ( 1 )
   {
     c = fgetc ( fp1 ) ; // reading the file
     if ( c == EOF )
     break ;
     if(counter>-1) //Write into first file
     {
     fwrite(&c , sizeof(c), 1, fp2);
     printf("Checking = %c " ,c );
     counter--;
     }
     else //Write into second file
     {
     fwrite(&c, sizeof(c), 1, fp3);
     printf("Checking2 = %c " ,c);
     }
   }
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
    return 0;
}

int combineFileContents(char first_file_path[], char second_file_path[])
{
    FILE* fp1, *fp2;
    char c;
   
    fp1 = fopen(first_file_path, "r");
    fp2 = fopen(second_file_path, "r");
    if (fp1 == NULL) {
        printf("File Not Found!\n");
        return -1;
    }
    if (fp2 == NULL) {
        printf("File Not Found!\n");
        return -1;
    }
    while ( 1 )
   {
     c = fgetc ( fp1 ) ; // reading the file
     if ( c == EOF )
     break ;
     printf("%c", c);
   }
   while ( 1 )
   {
     c = fgetc ( fp2 ) ; // reading the file
     if ( c == EOF )
     break ;
     printf("%c", c);
   }
   fclose(fp1);
   fclose(fp2);
    return 0;
}
int CreateParityP(char first_file_path[], char second_file_path[],char P_file_path[])
{
    FILE* f1, * f2, *fxor;
    char c1,c2;
    f1 = fopen(first_file_path, "r");
    f2 = fopen(second_file_path, "r");
    fxor = fopen(P_file_path, "w");
    while ( 1 )
   {
     c1 = fgetc ( f1 ) ; // reading the file 1
     c2 = fgetc ( f2 ) ; // reading the file 2
     if ( c1 != EOF && c2!= EOF )
     {
         //printf("First = %c and Second = %c \n",c1,c2);
         int temp = XOR_Encode(c1,c2);
         //printf("answer = %d \n", temp);

         //Making it equal to three characters
         char str[10];
         sprintf(str, "%d", temp); 
         char final[10]=""; 
         if(strlen(str)<2){  
            strcpy(final, "00");
            strcat(final, str);
            //printf("Lessgoo = %s ",final);
            fputs(final,fxor);
            }
         else if (strlen(str)<3){
            strcpy(final, "0");
            strcat(final, str);
            //printf("Lessgoo = %s ",final);
            fputs(final,fxor);
            //fwrite(&final , sizeof(final), 1, fxor);
            }
         else{
             //printf("Lessgoo = %s ",str);
             fputs(str,fxor);
             //fwrite( &str, sizeof(str), 1, fxor);

         }
     }
     else break;
   }
    fclose(f1);
    fclose(f2);
    fclose(fxor);
    return 0;
}
int CreateParityQ(char first_file_path[], char second_file_path[],char Q_file_path[])
{
    FILE* f1, * f2, *fQ;
    char c1,c2;
    f1 = fopen(first_file_path, "r");
    f2 = fopen(second_file_path, "r");
    fQ = fopen(Q_file_path, "w");
    while ( 1 )
   {
     c1 = fgetc ( f1 ) ; // reading the file 1
     c2 = fgetc ( f2 ) ; // reading the file 2
     if ( c1 != EOF && c2!= EOF )
     {
         printf("First = %c and Second = %c \n",c1,c2);
         int temp = XOR_Encode(c1,2*c2);
         printf("answer = %d \n", temp);
         char str[10];
         sprintf(str, "%d", temp); 
         char final[10]=""; 
         if(strlen(str)<2){  
            strcpy(final, "00");
            strcat(final, str);
            //printf("Lessgoo = %s ",final);
            fputs(final,fQ);
            }
         else if (strlen(str)<3){
            strcpy(final, "0");
            strcat(final, str);
            //printf("Lessgoo = %s ",final);
            fputs(final,fQ);
            //fwrite(&final , sizeof(final), 1, fxor);
            }
         else{
             //printf("Lessgoo = %s ",str);
             fputs(str,fQ);
             //fwrite( &str, sizeof(str), 1, fxor);

         }
     }
     else break;
   }
    fclose(f1);
    fclose(f2);
    fclose(fQ);
    return 0;
}

int decode_one_fault()
{
    //assuming ErasureCoding4.txt" crashed
    FILE* f1, * fxor;
    char c,c1;
    f1 = fopen("/Users/saamajanaraharisetti/ErasureCoding3.txt", "r");
    fxor = fopen("/Users/saamajanaraharisetti/ErasureCodingPlease.txt", "r");
    while(1)
   {
     char buffer[4];
     if(fgets (buffer, 4, fxor) ==NULL)
     break;
     //printf("lets decode = %s \n" ,buffer);
     int xor_number = atoi(buffer);
     //printf("xor_num=%d\n",xor_number);
     c1 = fgetc ( f1 ) ;
     //printf("char to int = %d",(int)c1);
     int temp = XOR_Decode(xor_number,(int)c1);
     //printf("%c",(char)temp);
   }
    fclose(fxor);
    fclose(f1);
    return 0;
}

int decode_when_fileA_fileP_are_lost()
{
    FILE* fB, * fQ;
    char c,c1;
    fB = fopen("/Users/saamajanaraharisetti/ErasureCoding4.txt", "r");
    fQ = fopen("/Users/saamajanaraharisetti/ErasureCodingParityQ.txt", "r");
    while(1)
   {
     char buffer[4];
     if(fgets (buffer, 4, fQ) ==NULL)
     break;
     //printf("Stuff in Q = %s \n" ,buffer);
     int xor_number = atoi(buffer);
     //printf("xor_num=%d\n",xor_number);
     c1 = fgetc ( fB ) ;
     //printf("char to int = %d",(int)c1);
     int temp = XOR_Decode(xor_number,2* ((int)c1));
     printf("%c",(char)temp);
   }
    fclose(fB);
    fclose(fQ);
    return 0;
}

int decode_when_fileB_fileP_are_lost()
{
    FILE* fA, * fQ;
    char c,c1;
    fA = fopen("/Users/saamajanaraharisetti/ErasureCoding3.txt", "r");
    fQ = fopen("/Users/saamajanaraharisetti/ErasureCodingParityQ.txt", "r");
    while(1)
   {
     char buffer[4];
     if(fgets (buffer, 4, fQ) ==NULL)
     break;
     //printf("Stuff in Q = %s \n" ,buffer);
     int xor_number = atoi(buffer);
     //printf("xor_num=%d\n",xor_number);
     c1 = fgetc ( fA ) ;
     //printf("char to int = %d",(int)c1);
     int temp = XOR_Decode(xor_number,(int)c1);
     printf("%c",(char)(temp/2));
   }
    fclose(fA);
    fclose(fQ);

    return 0;

}
int decode_when_fileA_fileB_are_lost()
{
    FILE* fP, * fQ;
    char c,c1;
    fP = fopen("/Users/saamajanaraharisetti/ErasureCodingPlease.txt", "r");
    fQ = fopen("/Users/saamajanaraharisetti/ErasureCodingParityQ.txt", "r");
    while(1)
   {
     char bufferP[4];
     if(fgets (bufferP, 4, fP) ==NULL)
     break;
     char bufferQ[4];
     if(fgets (bufferQ, 4, fQ) ==NULL)
     break;
     int P_num = atoi(bufferP);
     int Q_num = atoi(bufferQ);
     int tempA = XOR_Decode(2*P_num,Q_num);
     printf("A = %c   ",(char)tempA);
     int tempB = XOR_Decode(Q_num,P_num);
     printf("B = %c\n",(char)tempB);

   }
    fclose(fP);
    fclose(fQ);
    return 0;
}

int XOR_Decode(int x, int y)
{
    while (y != 0)
    {
        int borrow = (~x) & y;
        x = x ^ y;
        y = borrow << 1;
    }
    return x;
}
int XOR_Encode(int x, int y)
{
    while (y != 0)
    {
        unsigned carry = x & y;
        x = x ^ y;
        y = carry << 1;
    }
    return x;
}
