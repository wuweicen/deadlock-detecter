#define _GNU_SOURCE
#include <dlfcn.h>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>

#define THREAD_NUM 10

typedef unsigned long int uint64;

typedef int (*pthread_mutex_lock_t)(pthread_mutex_t *mutex);

pthread_mutex_lock_t pthread_mutex_lock_f;

typedef int (*pthread_mutex_unlock_t)(pthread_mutex_t *mutex);

pthread_mutex_unlock_t pthread_mutex_unlock_f;

#if 1 // graph

#define MAX 100

enum Type { PROCESS, RESOURCE }; //进程，资源

//结点数据
struct source_type {
  uint64 id;      //线程id
  enum Type type; //结点类型（没有使用-不用考虑这个类型信息）
  uint64 lock_id; // mutex id
  int degress; //当前 mutex id 正在被线程lock的标志（我可能理解的有问题）
};

//结点
struct vertex {
  struct source_type s;
  struct vertex *next; //结点与结点之间的边的链表表示
};

//图-邻接表表示
struct task_graph {
  struct vertex list[MAX]; //结点列表
  int num;                 //结点数量
  struct source_type locklist
      [MAX]; //所有线程对应的锁的列表（一个线程可能有多个锁，那么就是一个线程就有多个记录，）
  int lockidx; //锁 id下标
  pthread_mutex_t mutex;
};

struct task_graph *tg = NULL; //图的表示
int path[MAX + 1];            //访问的线程id路径
int visited[MAX];             //标识结点是否被访问的标志
int k = 0;                    //用来存放路径的序号
int deadlock = 0;             //死锁标志

//创建结点
struct vertex *create_vertex(struct source_type type) {

  struct vertex *tex = (struct vertex *)malloc(sizeof(struct vertex));

  tex->s = type;
  tex->next = NULL; //

  return tex;
}

//搜索结点，存在返回下标，否则返回-1
int search_vertex(struct source_type type) {

  int i = 0;

  for (i = 0; i < tg->num; i++) {

    if (tg->list[i].s.type == type.type &&
        tg->list[i].s.id ==
            type.id) { //结点类型（不考虑）和结点id共同决定一个顶点
      return i;
    }
  }

  return -1;
}

//增加结点
void add_vertex(struct source_type type) {

  if (search_vertex(type) == -1) { //不存在结点，则添加

    tg->list[tg->num].s = type;
    tg->list[tg->num].next = NULL;
    tg->num++;
  }
}

//增加边
void add_edge(struct source_type from, struct source_type to) {

  add_vertex(from);
  add_vertex(to);

  struct vertex *v = &(tg->list[search_vertex(from)]); //找到数组下标位置

  while (v->next != NULL) { //将边插入到当前结点的所有出边之后
    v = v->next;
  }

  v->next = create_vertex(to);
}

//验证结点i和结点j之间是否存在边
int verify_edge(struct source_type i, struct source_type j) {

  if (tg->num == 0)
    return 0;

  int idx = search_vertex(i);
  if (idx == -1) {
    return 0;
  }

  struct vertex *v = &(tg->list[idx]);
  //遍历i的所有出边，直到找到为止
  while (v != NULL) {

    if (v->s.id == j.id)
      return 1;

    v = v->next;
  }

  return 0;
}

//删除边 from-起点 to-终点
void remove_edge(struct source_type from, struct source_type to) {

  int idxi = search_vertex(from);
  int idxj = search_vertex(to);

  if (idxi != -1 && idxj != -1) {

    struct vertex *v = &tg->list[idxi];
    struct vertex *remove;

    while (v->next != NULL) {
      //找到要删除的边
      if (v->next->s.id == to.id) {

        remove = v->next;
        v->next = v->next->next;

        free(remove);
        break;
      }
      v = v->next;
    }
  }
}

void print_deadlock(void) {

  int i = 0;

  printf("deadlock : ");
  for (i = 0; i < k - 1; i++) {

    printf("%ld --> ", tg->list[path[i]].s.id);
  }

  printf("%ld\n", tg->list[path[i]].s.id);
}

//深度优先搜索-idx:结点序号（要遍历的第一个结点）
int DFS(int idx) {

  struct vertex *ver = &tg->list[idx];
  if (visited[idx] == 1) { //只有该结点是遍历过，那么就是存在环（死锁）
    path[k++] = idx;
    print_deadlock();
    deadlock = 1;
    return 0;
  }
  visited[idx] = 1; //遍历过后就将该结点标志设置为1
  path[k++] = idx;
  while (ver->next != NULL) {
    DFS(search_vertex(ver->next->s)); //深度遍历下一个结点
    k--; // idx的某一条边的所有已经遍历完成，路径有回退的操作.这里有点不太好理解，可以自己画一个很简单的有向图进行分析，
    ver = ver->next; //指向该节点的下一条边
  }
  return 1;
}

//从idx结点开始遍历，看是否存在环
void search_for_cycle(int idx) {

  struct vertex *ver = &tg->list[idx];
  visited[idx] = 1;
  k = 0;
  path[k++] = idx; //记下idx序号结点的路径编码

  while (ver->next != NULL) {
    int i = 0;
    for (i = 0; i < tg->num; i++) { //除该节点之外的所有结点将其标志设置设置为0
      if (i == idx)
        continue;
      visited[i] = 0;
    }

    for (i = 1; i <= MAX; i++) {
      path[i] =
          -1; //路径编码设置为-1，代表除起始节点的所有结点还不存在路径即：path[max]={idx,-1.-1.-1.-1......}
    }
    k = 1;
    DFS(search_vertex(ver->next->s)); //从该节点的某个边的节点开始遍历
    ver = ver->next;
  }
}

#if 0
int main() {

        tg = (struct task_graph*)malloc(sizeof(struct task_graph));
        tg->num = 0;

        struct source_type v1;
        v1.id = 1;
        v1.type = PROCESS;
        add_vertex(v1);

        struct source_type v2;
        v2.id = 2;
        v2.type = PROCESS;
        add_vertex(v2);

        struct source_type v3;
        v3.id = 3;
        v3.type = PROCESS;
        add_vertex(v3);

        struct source_type v4;
        v4.id = 4;
        v4.type = PROCESS;
        add_vertex(v4);


        struct source_type v5;
        v5.id = 5;
        v5.type = PROCESS;
        add_vertex(v5);


        add_edge(v1, v2);
        add_edge(v2, v3);
        add_edge(v3, v4);
        add_edge(v4, v5);
        add_edge(v3, v1);

        search_for_cycle(search_vertex(v1));

}
#endif

#endif

//检测是否有死锁
void check_dead_lock(void) {

  int i = 0;

  deadlock = 0;
  for (i = 0; i < tg->num; i++) { //对所有结点进行遍历-看结点是否存在环
    if (deadlock == 1)
      break;
    search_for_cycle(i);
  }

  if (deadlock == 0) {
    printf("no deadlock\n");
  }
}

//检测是否有死锁的线程
static void *thread_routine(void *args) {

  while (1) {
    sleep(5);
    check_dead_lock();
  }
}

void start_check(void) {

  tg = (struct task_graph *)malloc(sizeof(struct task_graph));
  tg->num = 0;
  tg->lockidx = 0;
  pthread_t tid;
  pthread_create(&tid, NULL, thread_routine, NULL);
}

#if 1
// lock:mutex id
//判断mutex id是否在所有的锁列表中
int search_lock(uint64 lock) {

  int i = 0;

  for (i = 0; i < tg->lockidx; i++) {

    if (tg->locklist[i].lock_id == lock) {
      return i;
    }
  }

  return -1;
}

// lock:mutex id
//
int search_empty_lock(uint64 lock) {

  int i = 0;

  for (i = 0; i < tg->lockidx; i++) {

    if (tg->locklist[i].lock_id == 0) {
      return i;
    }
  }
  return tg->lockidx;
}

#endif

//原子操作-++操作（这里涉及到底层码-没必要细细研究）
int inc(int *value, int add) {

  int old;
  __asm__ volatile("lock;xaddl %2, %1;"
                   : "=a"(old)
                   : "m"(*value), "a"(add)
                   : "cc", "memory");

  return old;
}

void print_locklist(void) {
  int i = 0;
  printf("print_locklist: \n");
  printf("---------------------\n");
  for (i = 0; i < tg->lockidx; i++) {
    printf("threadid : %ld, lockid: %ld\n", tg->locklist[i].id,
           tg->locklist[i].lock_id);
  }
  printf("---------------------\n\n\n");
}

// thread_id:线程id
// lockaddr:线程即将要加的mutex id
void lock_before(uint64 thread_id, uint64 lockaddr) {

  int idx = 0;
  // list<threadid, toThreadid>
  for (idx = 0; idx < tg->lockidx; idx++) {
    if (tg->locklist[idx].lock_id == lockaddr) {

      struct source_type from;
      from.id = thread_id;
      from.type = PROCESS;
      add_vertex(from);

      struct source_type to;
      to.id = tg->locklist[idx].id;
      tg->locklist[idx].degress++;
      to.type = PROCESS;
      add_vertex(to);

      if (!verify_edge(from, to)) {
        add_edge(from, to); //
      }
    }
  }
}

void lock_after(uint64 thread_id, uint64 lockaddr) {

  int idx = 0;
  if (-1 == (idx = search_lock(lockaddr))) { // 该锁还没被其他线程lock过

    int eidx = search_empty_lock(lockaddr);

    tg->locklist[eidx].id = thread_id;
    tg->locklist[eidx].lock_id = lockaddr;

    inc(&tg->lockidx, 1); // lock id下标增加+1

  } else { // idx不为-1，说明该锁被某个线程lock过，只是被该线程unlock了

    struct source_type from;
    from.id = thread_id;
    from.type = PROCESS;

    struct source_type to;
    to.id = tg->locklist[idx].id;
    tg->locklist[idx].degress--;
    to.type = PROCESS;

    if (verify_edge(from, to)) //判断是否存在边，如果存在对应的边
      remove_edge(from, to);

    tg->locklist[idx].id = thread_id; //设置对应下标的锁的线程id为当前锁的线程id
  }
}

void unlock_after(uint64 thread_id, uint64 lockaddr) {

  int idx = search_lock(lockaddr);

  if (tg->locklist[idx].degress == 0) {
    tg->locklist[idx].id = 0;
    tg->locklist[idx].lock_id = 0;
    // inc(&tg->lockidx, -1);
  }
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {

  pthread_t selfid = pthread_self();  //
  lock_before(selfid, (uint64)mutex); //加锁前操作
  pthread_mutex_lock_f(mutex);
  lock_after(selfid, (uint64)mutex); //加锁后操作

  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {

  pthread_t selfid = pthread_self();
  pthread_mutex_unlock_f(mutex);
  unlock_after(selfid, (uint64)mutex); //解锁后操作

  return 0;
}

static void init_hook() {
  //锁住系统函数 pthread_mutex_lock
  pthread_mutex_lock_f =
      (pthread_mutex_lock_t)dlsym(RTLD_NEXT, "pthread_mutex_lock");
  pthread_mutex_unlock_f =
      (pthread_mutex_lock_t)dlsym(RTLD_NEXT, "pthread_mutex_unlock");
}

#if 1 // debug
pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_4 = PTHREAD_MUTEX_INITIALIZER;

void *thread_rountine_1(void *args) {
  pthread_t selfid = pthread_self(); //

  printf("thread_routine 1 : %ld \n", selfid);

  pthread_mutex_lock(&mutex_1); //执行自定义的函数
  sleep(1);
  pthread_mutex_lock(&mutex_2);

  pthread_mutex_unlock(&mutex_2);
  pthread_mutex_unlock(&mutex_1);

  return (void *)(0);
}

void *thread_rountine_2(void *args) {
  pthread_t selfid = pthread_self(); //

  printf("thread_routine 2 : %ld \n", selfid);

  pthread_mutex_lock(&mutex_2);
  sleep(1);
  pthread_mutex_lock(&mutex_3);

  pthread_mutex_unlock(&mutex_3);
  pthread_mutex_unlock(&mutex_2);

  return (void *)(0);
}

void *thread_rountine_3(void *args) {
  pthread_t selfid = pthread_self(); //

  printf("thread_routine 3 : %ld \n", selfid);

  pthread_mutex_lock(&mutex_3);
  sleep(1);
  pthread_mutex_lock(&mutex_4);

  pthread_mutex_unlock(&mutex_4);
  pthread_mutex_unlock(&mutex_3);

  return (void *)(0);
}

void *thread_rountine_4(void *args) {
  pthread_t selfid = pthread_self(); //

  printf("thread_routine 4 : %ld \n", selfid);

  pthread_mutex_lock(&mutex_4);
  sleep(1);
  pthread_mutex_lock(&mutex_1);

  pthread_mutex_unlock(&mutex_1);
  pthread_mutex_unlock(&mutex_4);

  return (void *)(0);
}

int main() {
  init_hook();
  start_check();
  printf("start_check\n");
  pthread_t tid1, tid2, tid3, tid4;
  pthread_create(&tid1, NULL, thread_rountine_1, NULL);
  pthread_create(&tid2, NULL, thread_rountine_2, NULL);
  pthread_create(&tid3, NULL, thread_rountine_3, NULL);
  pthread_create(&tid4, NULL, thread_rountine_4, NULL);
  pthread_join(tid1, NULL); //等待线程1的结束
  pthread_join(tid2, NULL);
  pthread_join(tid3, NULL);
  pthread_join(tid4, NULL);
  return 0;
}
#endif
