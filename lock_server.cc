// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, lock_protocol::status &r)
{
  printf("acquire request of lock %016llx\n", lid);
  pthread_mutex_lock(&mutex);
  //printf("p0\n");
  nacquire ++;
  std::map<lock_protocol::lockid_t, bool>::iterator it = lock_list.find(lid);
  //printf("p1\n");
  if (it == lock_list.end()) {
    //printf("%d generating new lock %d\n", clt, lid);
    lock_list.insert(std::pair<lock_protocol::lockid_t, bool>(lid, false));
    // TODO
    pthread_cond_t cond;
    lock_cond_list[lid] = cond;
    pthread_cond_init(&lock_cond_list[lid], NULL);
  }
  
  // TODO 
  pthread_cond_t &cond = lock_cond_list[lid];
  //printf("%d\n", it->second);
  while (it->second) {
    //printf("%d wait lock %d\n", clt, lid);
    pthread_cond_wait(&cond, &mutex);
    //printf("%d get signal to free lock %d\n", clt, lid);
  }
  lock_list[lid] = true;
  //printf("p2\n");
  pthread_mutex_unlock(&mutex);

  lock_protocol::status ret = lock_protocol::OK;
  r = lock_protocol::OK;
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, lock_protocol::status &r)
{
  printf("release request of lock %016llx\n", lid);
  pthread_mutex_lock(&mutex);
  //printf("p0\n");
  nacquire --;
  std::map<lock_protocol::lockid_t, bool>::iterator it = lock_list.find(lid);
  //printf("p1\n");
  if (it == lock_list.end()) {
    //printf("%d generating new lock %d\n", clt, lid);
    lock_list.insert(std::pair<lock_protocol::lockid_t, bool>(lid, false));
    // TODO
    pthread_cond_t cond;
    lock_cond_list[lid] = cond;
    pthread_cond_init(&lock_cond_list[lid], NULL);
  }
  
  // TODO 
  pthread_cond_t &cond = lock_cond_list[lid];
  //printf("%d\n", it->second);
  if (it->second) {
    //printf("%d release lock %d\n", clt, lid);
    pthread_cond_broadcast(&cond);
    //printf("%d released lock %d\n", clt, lid);
  }
  lock_list[lid] = false;
  //printf("p2\n");
  pthread_mutex_unlock(&mutex);

  lock_protocol::status ret = lock_protocol::OK;
  r = lock_protocol::OK;
  return ret;
}


