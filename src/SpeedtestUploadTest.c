#include "SpeedtestUploadTest.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

static void __appendTimestamp(const char *url, char *buff, int buff_len)
{
    char delim = '?';
    char *p = strchr(url, '?');

    if (p)
        delim = '&';
    snprintf(buff, buff_len, "%s%cx=%llu", url, delim, (unsigned long long)time(NULL));
}

static void *__uploadThread(void *arg)
{
    /* Testing upload... */
    THREADARGS_T *threadConfig = (THREADARGS_T *)arg;
    char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buffer[BUFFER_SIZE] = {0};
    int i, size;
    struct timeval tval_start;
    unsigned long totalTransfered = 0;
    char uploadUrl[1024];
    sock_t sockId;

    /* Build the random buffer */
    printf("[%d]...build the random buffer...\n", threadConfig->tid);
    srand(time(NULL));
    for(i=0; i < BUFFER_SIZE; i++)
    {
        buffer[i] = alphabet[rand() % ARRAY_SIZE(alphabet)];
    }

    gettimeofday(&tval_start, NULL);
    printf("[%d]...starting %d tests\n", threadConfig->testCount);
    for (i = 0; i < threadConfig->testCount; i++)
    {
        printf("[%d]...test no %d\n", threadConfig->tid, i);
        __appendTimestamp(threadConfig->url, uploadUrl, sizeof(uploadUrl));
        printf("[%d]...cp1\n", threadConfig->tid);
        /* FIXME: totalToBeTransfered should be readonly while the upload thread is running */
        totalTransfered = totalToBeTransfered;
        printf("[%d]...cp2\n", threadConfig->tid);
        sockId = httpPutRequestSocket(uploadUrl, totalToBeTransfered);
        printf("[%d]...cp3\n", threadConfig->tid);
        if(sockId == 0)
        {
            printf("Unable to open socket for Upload!");
            pthread_exit(NULL);
        }
        printf("[%d]...cp4\n", threadConfig->tid);

        while(totalTransfered != 0)
        {
            if (totalTransfered > BUFFER_SIZE)
            {
                size = httpSend(sockId, buffer, BUFFER_SIZE);
            }
            else
            {
                buffer[totalTransfered - 1] = '\n'; /* Indicate terminated */
                size = httpSend(sockId, buffer, totalTransfered);
            }

            totalTransfered -= size;
        }
        printf("[%d]...cp5\n", threadConfig->tid);
        threadConfig->transferedBytes += totalToBeTransfered;
        /* Cleanup */
        printf("[%d]...cleanup http\n", threadConfig->tid);
        httpClose(sockId);
        printf("[%d]...done cleanup http\n", threadConfig->tid);
    }
    threadConfig->elapsedSecs = getElapsedTime(tval_start);

    return NULL;
}

void testUpload(const char *url)
{
    size_t numOfThreads = speedTestConfig->uploadThreadConfig.threadsCount;
    THREADARGS_T *param = (THREADARGS_T *) calloc(numOfThreads, sizeof(THREADARGS_T));

    int i;
    for (i = 0; i < numOfThreads; i++)
    {
        /* Initializing some parameters */
        param[i].testCount =  speedTestConfig->uploadThreadConfig.length;
        if (param[i].testCount == 0)
        {
            /* At least three test should be run */
            param[i].testCount = 3;
        }
        param[i].url = strdup(url);
        if (param[i].url)
        {
            pthread_create(&param[i].tid, NULL, &__uploadThread, &param[i]);
        }
    }

    /* Refresh */
    totalTransfered = 0;
    float speed = 0;

    /* Wait for all threads */
    printf("...wait for threads\n");
    for (i = 0; i < numOfThreads; i++)
    {
        printf("...waiting for %d (tid: %d)\n", i, param[i].tid);
        pthread_join(param[i].tid, NULL);
        printf("...joined a thread\n");
        if (param[i].transferedBytes)
        {
            /* There's no reason that we transfered nothing except error occured */
            totalTransfered += param[i].transferedBytes;
            speed += (param[i].transferedBytes / param[i].elapsedSecs) / 1024;
        }
        /* Cleanup */
        printf("...cleanup url\n");
        free(param[i].url);
        printf("...done with thread\n");
    }
    free(param);
    printf("Bytes %lu uploaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
        totalTransfered, speed, speed * 8 / 1024);
}
