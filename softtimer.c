/* 
MIT License

Copyright (c) 2017 Yuandong-Chen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <time.h>

typedef void Func(int);

/* __timer struct is the real Timer struct we use
 * id is unique to each timer
 * intersec is the inteval seconds to each signal forwarding the this Timer
 * sigactor is the handler for this Timer
 * next is a internal member used for linked list
 */
struct __timer
{
    void *next;
    unsigned int sec;
    unsigned int intersec;
    int id;
    Func *sigactor;
};

/* struct alarm is ugly for the compatibility with early struct.
 * I should have used unnamed member instead of __inner.
 */
typedef struct alarm *Timer;
struct alarm
{
    union{
        struct
        {
            Timer next;
            unsigned int sec;
        };
        struct __timer __inner;
    }; 
};

typedef struct list *Header;

struct list
{
    Timer head;
};

struct list linkedlist;
Header hdr_ptr = &linkedlist;


Timer mallocTimer(int id, Func *actor,unsigned int sec, unsigned int interval)
{
    Timer ret = (Timer)malloc(sizeof(struct alarm));
    assert(ret);
    ret->__inner.id = id;
    ret->__inner.sigactor = actor;
    ret->__inner.intersec = interval;
    ret->sec = sec;
    return ret;
}

/* find Timer in linked list which id is id.
 * return: return NULL if not found, -1 if it's header link, 
 * otherwise prev which is the previous Timer member to this Timer
 */

Timer findTimerPrev(Header h, int id)
{
    assert(h);
    if(h->head == NULL)
        return NULL;

    Timer t = h->head;
    Timer prev = NULL;

    while(t)
    {
        if(t->__inner.id == id){
            if(prev == NULL)
                return (Timer)-1;
            else
                return prev;
        }
        prev = t;
        t = t->next;
    }

    return NULL;
}

/* delete Timer in linked list.
 * return: nothing, we ensure this t is deleted in the linked list.
 */

void delTimer(Header h, Timer t)
{
    assert(h);
    assert(t);
    Timer prevtodel = findTimerPrev(h, t->__inner.id);
    unsigned int base = 0;

    if(prevtodel)
    {
        if(prevtodel == (Timer)-1){

            unsigned int res = (h->head)->sec;
            if(res != 0)
            {
                base = res;
            }
            else
            {
                kill(getpid(),SIGALRM);
                return;
            }
            h->head = (h->head)->next;
            Timer tmp = (h->head);

            while(tmp){
                tmp->sec += base;
                tmp = tmp->next;
            }
            return;
        }
        else
        {
            
            base = (prevtodel->next)->sec;
            prevtodel->next = (prevtodel->next)->next;
            Timer tmp = (prevtodel->next);
            
            while(tmp){
                tmp->sec += base;
                tmp = tmp->next;
            }
            return;
        }
    }

    return;
}

/* append Timer in appropriate place in linked list.
 * the appropriate place means all timers in linked list are arranged 
 * according their next alarm seconds.
 * The algorithm we use here is that the real left alarm seconds for this Timer 
 * is the sum of all the sec member in Timer in linked list prev to this Timer
 * plus its sec member. For example, we add 3 Timers to the linked list,
 * whose sec are 4, 3, 2 respectively. Then the linked list looks like:
 * 2 (real sec = 2) --> 1 (real sec = 2+1 = 3) --> 1 (real sec = 2+1+1 = 4)
 * The advantage is obviously, we dont need to remember how many seconds passed.
 * We always fetch the header to respond the alarm signal and set next alarm sec 
 * as the next timer in the linked list. (The real situation is a little bit more 
 * complex, for example if upcoming timers' sec equals 0, we need to call their
 * handler right away all together in a certain sequence. If its intersec is not 
 * zero, we need to append it to the linked list again as quick as possible)
 * note: delTimer also address this problem. If we delete any Timer, we need to 
 * recalculate the secs after this timer in the linked list.(simply to add sec to 
 * the next timer and delete this timer node)
 * return: only 0 if success, otherwise the hole process failed.
 */

int appendTimer(Header h, Timer t)
{
    assert(h);
    assert(t);
    delTimer(h, t);

    if(h->head == NULL)
    {
        h->head = t;
        return 0;
    }

    Timer tmp = h->head;
    Timer prev = NULL;
    unsigned int prevbase = 0;
    unsigned int base = 0;

    while(tmp)
    {
        prevbase = base;
        base += tmp->sec;
        if(t->sec < base){
            break;
        }
        else{
            prev = tmp;
            tmp = tmp->next;
        }
            
    }

    if(prev == NULL)
    {
        (h->head)->sec -= t->sec;
        t->next = h->head;
        h->head = t;
        return 0;
    }

    if(tmp == NULL)
        t->sec -=base;
    else
        t->sec -=prevbase;

    prev->next = t;
    t->next = tmp;
    if(tmp)
        tmp->sec -= t->sec;

    return 0;
}

/* pop header timer in linked list.
 * return: its hander
 */

Func* popTimer(Header h)
{
    assert(h);
    if(h->head == NULL)
        return (Func *)-1;
    Func *ret = (h->head)->__inner.sigactor;
    Timer todel = h->head;
    h->head = (h->head)->next;
    // if its intersec greater than 0, we append it right away to the linked list
    if(todel->__inner.intersec > 0)
    {
        todel->sec = todel->__inner.intersec;
        appendTimer(h, todel);
    }
    return ret;
}

void printList(Header h)
{
    assert(h);
    if(h->head == NULL)
        return;

    Timer tmp = h->head;

    while(tmp)
    {
        printf("timer[%d] = %u saved %u\n", tmp->__inner.id, tmp->sec, tmp->__inner.intersec);
        tmp = tmp->next;
    }
}

/* it's the real signal handler responding to every SIGALRM.
 */

static void sig_alarm_internal(int signo)
{ 
    void funcWrapper(int signo, Func *func);

    if(hdr_ptr->head == NULL)
        return;

    Func *recv;
    if((recv = popTimer(hdr_ptr)) == (Func *)-1){
        funcWrapper(SIGALRM, recv);
    }  
    else
    {
        // signal ourself if next timer's sec = 0
        if(hdr_ptr->head){
            ((hdr_ptr->head)->sec > 0?alarm((hdr_ptr->head)->sec):kill(getpid(), SIGALRM));
        }
        funcWrapper(SIGALRM, recv);
    }
}

/* Alarm function simulates native alarm function.
 * what if SIGALRM arrives when process is running in Alarm?
 * we just block the signal since there is no slow function in Alarm,
 * sig_alarm_internal will for sure address the signal very soon.
 */

unsigned int Alarm(Header h, Timer mtimer)
{
    sigset_t mask;
    sigset_t old;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &old);
    
    unsigned int res = 0;
    Timer t;

    if((t = findTimerPrev(h, mtimer->__inner.id)) == NULL)
        goto LL;

    t = h->head;
    while(t)
    {
        res += t->sec; // it's not precise, we should use alarm(0) for the first sec.
                       // However, its simple enough to implement. 
        if(t->__inner.id == mtimer->__inner.id)
            break;

        t = t->next;
    }
LL:
    if(mtimer->sec == 0)
    {
        delTimer(h, mtimer);
        sigprocmask(SIG_SETMASK, &old, NULL);
        return res;
    }
     
    appendTimer(h, mtimer);
    if(mtimer->__inner.id == (h->head)->__inner.id)
        ((h->head)->sec > 0?alarm((h->head)->sec):kill(getpid(), SIGALRM));
    sigprocmask(SIG_SETMASK, &old, NULL);
    return res;
}

static void init()
{
    struct sigaction act;
    act.sa_handler = sig_alarm_internal;
    act.sa_flags = SA_RESTART|SA_NODEFER;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

void funcWrapper(int signo, Func *func)
{
    sigset_t mask;
    sigset_t old;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &mask, &old);
    func(signo);
    sigprocmask(SIG_SETMASK, &old, NULL);
}

////////////////////////       Test       //////////////////////////////
volatile clock_t new1, new2, new3, old1, old2, old3;

void signal_forfun(signo)
{
    new1 = clock();
    printf("fun! %.4f seconds\n", ((double)(new1 - old1)/CLOCKS_PER_SEC));
    old1 = new1;
}

void signal_forhello(signo)
{ 
    new3 = clock();
    printf("hello! %.4f seconds\n", ((double)(new3 - old3)/CLOCKS_PER_SEC));
    old3 = new3;
}

void signal_forhi(signo)
{
    new2 = clock();
    printf("hi! %.4f seconds\n", ((double)(new2 - old2)/CLOCKS_PER_SEC));
    old2 = new2;
}

int
main(void)
{
    new1 =new2 =new3 = old1 =old2 = old3=clock();

    init();
    /* 
     * params init in Timer : 
     * id (unique to each Timer),
     * handler (associated to this Timer), 
     * delay ( =0 to cancel this Timer, others to delay x sec to alarm this Timer),
     * inteval( =0 to alarm only once, others to alarm every x sec to this Timer)
     */

    /*
     * params in Alarm :
     * linked list pointer, used to save all the Timer.
     * The inserted Timer pointer, 
     * when id is already existed in the link, replace it (a possible leak).
     */
    Alarm(hdr_ptr,mallocTimer(1, signal_forhi, 3, 1)); 
    Alarm(hdr_ptr,mallocTimer(2, signal_forfun, 2, 2));
    Alarm(hdr_ptr,mallocTimer(3, signal_forhello, 1, 3));
    
    while(1){};

    exit(0); 
}
