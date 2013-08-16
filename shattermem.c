#define SLEEPTIME 3600 // How long to wait before exiting (in seconds)
#define MINTIME 60 // How long to wait before returning free memory
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

long unsigned int convertsizetolong (char *sizestr)
  {
  long unsigned int answer=0;
  int i=0;

  while(sizestr[i] != '\0')
    {
    if(sizestr[i] == '0')
      answer*=10;
    if(sizestr[i] >= '1' && sizestr[i] <= '9')
      {
      answer*=10;
      answer+=(sizestr[i]-'1'+1);
      }
    if(sizestr[i] == 'k' || sizestr[i] == 'K')
      answer*=1024;
    if(sizestr[i] == 'm' || sizestr[i] == 'M')
      answer*=(1024*1024);
    if(sizestr[i] == 'g' || sizestr[i] == 'G')
      answer*=(1024*1024*1024);
    i++;
    }
  return answer;
  }

int main (int argc, char **argv)
  {
  struct datanode
    {
    char *data;
    struct datanode *next;
    };
  char *totalmemsize_text = NULL;
  char *numchunks_text = NULL;
  char *chunksize_text = NULL;
  char *tempstr;
  char randstr[11];
  struct datanode *basenode;
  struct datanode *thisnode;
  struct datanode *tempnode;
  unsigned long totalmemsize = 0;
  unsigned long numchunks = 0;
  unsigned long chunksize = 0;
  int i=0;
  int j=0;
  int k;
  int errstate = 0;
  int isparent = 0;
  int opterr = 0;
  pid_t childPID;
  FILE *fp;

  while ((k = getopt (argc, argv, "t:n:s:")) != -1)
    switch (k)
      {
      case 't':
        totalmemsize_text = optarg;
        break;
      case 'n':
        numchunks_text = optarg;
        break;
      case 's':
        chunksize_text = optarg;
        break;
      case '?':
        errstate = 1;
        break;
      default:
        errstate = 1;
        break;
      }

  if(totalmemsize_text && numchunks_text && chunksize_text)
    errstate = 1;
  if(totalmemsize_text)
    {
    totalmemsize=convertsizetolong(totalmemsize_text);
    if(numchunks_text)
      {
      numchunks=convertsizetolong(numchunks_text);
      chunksize=totalmemsize/numchunks;
      }
    else if (chunksize_text)
      {
      chunksize=convertsizetolong(chunksize_text);
      numchunks=totalmemsize/chunksize;
      }
    else
      errstate = 1;
    }
  else
    {
    if(numchunks_text && chunksize_text)
      {
      numchunks=convertsizetolong(numchunks_text);
      chunksize=convertsizetolong(chunksize_text);
      totalmemsize=numchunks*chunksize;
      }
   else
     errstate = 1;
    }
  if(errstate)
    {
    printf("Usage: shattermem [-t SIZE] [-n nn] [-s SIZE]\n  where: -t is the total size of the memory you want to consume\n         -n is the number of chunks into which you want the memory split\n         -s is the size of the memory to allocate at once\n  SIZE can be as a number in k (kilobytes) M (megabytes) or G (gigabytes)\n  nn is an integer\n\n  Note that you must have EXACTLY 2 of the 3 arguments specified. The third will be computed. n*s=t.\n");
    return 0;
    }
  printf("totalmemsize=%lu; chunksize=%lu; numchunks=%lu \n",totalmemsize,chunksize,numchunks);

/*
 * Now that we have the numbers, time for some explanation.
 * If we just malloc() and free(), the memory doesn't go back to being
 * available by the OS. So we're going to create a new process here. Each
 * process will allocate half the memory (and since they're doing this at the
 * same time, they should intertwine reasonably well). Then, we'll kill off
 * one of the processes, making only fragmented memory available to the OS.
 */
  numchunks/=2; // each side of the fork gets half of the memory allocation
  childPID=fork();

  if(childPID >= 0) // fork was successful
    {
    if(childPID == 0)
      isparent=1;
    if (!(basenode = malloc(sizeof (struct datanode))))
      {
      if(isparent)
        tempstr="parent";
      else
        tempstr="child";
      printf ("Could not make initial memory allocation for %s.",tempstr);
      return -1;
      }
    thisnode=basenode;
    while((tempstr = malloc(chunksize)) && ((long unsigned int) i < numchunks))  // doing this as a while to immediately break out if the malloc fails
      {
      tempnode = malloc(sizeof (struct datanode));
      thisnode->data = tempstr;
      thisnode->next = tempnode;

/*
 * Linux is smart enough that it won't actually use the memory unless you write
 * something to it. So we grab 10 bytes from /dev/urandom and fill the memory
 * we just allocated with it over and over.
 */
      fp=fopen("/dev/urandom","r");
      fread(randstr,1,10,fp);
      fclose(fp);
      for(k=0; k<chunksize; k++)
        {
        tempstr[k]=randstr[j++];
        if(j == 10)
          j=0;
        }
// printf("done with chunk %d on PID %d",i,childPID);
      thisnode = tempnode;
      i++;
      }
    if(i < numchunks)
      printf("Failed to allocate memory on iteration %d. Giving up.\n",i);
    sleep(10); // make sure the other side is caught up
    if(childPID != 0) // Only do this for the child process
      {
      while(basenode != thisnode)
        {
	tempnode=basenode;
        basenode=tempnode->next;
	free(tempnode->data);
	free(tempnode);
	}
      return 0;
      }
    sleep (3600); // if this goes on longer than an hour without being <ctrl-c>'d, go ahead and kill it.
    while(basenode != thisnode)
      {
      tempnode=basenode;
      basenode=tempnode->next;
      free(tempnode->data);
      free(tempnode);
      }
    return 0;
    }
  else
    return -1;
  }
