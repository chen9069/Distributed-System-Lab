// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache():
	nacquire (0)
{
}

lock_server_cache::~lock_server_cache() {
}
/*
struct arg {
	lock_protocol::lockid_t lid;
	lock_server_cache * ls;
	long nn;
	arg(lock_protocol::lockid_t lid, lock_server_cache *ls, long nn) {
		this->lid = lid;
		this->ls = ls;
		this->nn = nn;
	}
};

void * 
retry_t(void *a) {
	lock_protocol::lockid_t lid = ((struct arg*)a)->lid; 
	long nn = ((struct arg*)a)->nn;
	lock_server_cache *ls = ((struct arg*)a)->ls;
printf("%d, %d, %d, %d\n", a, lid, nn, ls);
printf("%d\n", ls);
	ls->retry(lid);
}

void 
lock_server_cache::retry(lock_protocol::lockid_t lid) {
	pthread_mutex_lock(&mutex);
	pthread_cond_t cond = lock_cond_list[lid];
	printf("-------------------%016llx\n", lid);
	while (!handling) {
    pthread_cond_wait(&cond, &mutex);
		handling = true;
		while (lock_client_list[lid].size() < 2) {
		std::string client_1 = lock_client_list[lid].front();
  	sockaddr_in dstsock1;
  	make_sockaddr(client_1.c_str(), &dstsock1);
  	rpcc *cl1 = new rpcc(dstsock1);
  	if (cl1->bind() < 0) {
			printf("lock_client: call bind\n");
  	}
		lock_client_list[lid].pop_front();
		std::string client_2 = lock_client_list[lid].front();
		sockaddr_in dstsock2;
  	make_sockaddr(client_2.c_str(), &dstsock2);
  	rpcc *cl2 = new rpcc(dstsock2);
  	if (cl2->bind() < 0) {
			printf("lock_client: call bind\n");
  	}
		int rr;
		pthread_mutex_unlock(&mutex);
		tprintf("sending revoke to %s\n", client_1.c_str());
		rlock_protocol::status ret = cl1->call(rlock_protocol::revoke, lid, rr);
		VERIFY (ret == rlock_protocol::OK);
		tprintf("sending retry to %s\n", client_2.c_str());
		ret = cl2->call(rlock_protocol::retry, lid, rr);
		VERIFY (ret == rlock_protocol::OK);
		pthread_mutex_lock(&mutex);
		tprintf("get revoke/retry from %s/%s\n", client_1.c_str(), client_2.c_str());
		}	
		handling = false;
	}
	printf("----------------------thread: %016llx\n", lid);
	pthread_mutex_unlock(&mutex);
}
*/

lock_protocol::status
lock_server_cache::acquire(int clt, lock_protocol::lockid_t lid, std::string dst, 
                               int &r)
{
  pthread_mutex_lock(&mutex);
  //printf("%s acquire request of lock %016llx\n", dst.c_str(), lid);
  nacquire ++;
  r = nacquire;
	// null check
  if (lock_client_list.find(lid) == lock_client_list.end()) {
		std::vector<std::string> client_list;
		lock_client_list[lid] = client_list;
  }
	if (connection.find(dst)==connection.end()) {
  	sockaddr_in dstsock;
  	make_sockaddr(dst.c_str(), &dstsock);
		// TODO delete cl
  	rpcc *cl = new rpcc(dstsock);
  	if (cl->bind() < 0) {
			printf("lock_client: call bind\n");
  	}
		connection[dst] = cl;
	}

	int position = -1;
	for (int i = 0; i < lock_client_list[lid].size(); i ++) {
		if (lock_client_list[lid].at(i) == dst) {
			position = i;
			break;
		}
	}
	if (position < 0) {
		lock_client_list[lid].push_back(dst);
		position = lock_client_list[lid].size() - 1;
	}
//printf("---------------  %d\n", position);
	if (position == 0) {
  	pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	}
	else if (position == 1) {
		std::string client_dst = lock_client_list[lid].front();
  	rpcc *cl = connection[client_dst];
		int rr;
		//printf("%s sending revoke to %s\n", dst.c_str(), client_dst.c_str());
		lock_client_list[lid].erase(lock_client_list[lid].begin());
		pthread_mutex_unlock(&mutex);
// acq1 (np)
		rlock_protocol::status ret = cl->call(rlock_protocol::revoke, lid, rr);
// acq2 (np)
		VERIFY (ret == rlock_protocol::OK);
		pthread_mutex_lock(&mutex);
		//printf("%s get revoke from %s\n", dst.c_str(), client_dst.c_str());
		if (lock_client_list[lid].size() > 1) {
			client_dst = lock_client_list[lid].at(1);
  		cl = connection[client_dst];
			//printf("%s sending retry to %s\n", dst.c_str(), client_dst.c_str());
			pthread_mutex_unlock(&mutex);
// acq3 (np)
			rlock_protocol::status ret = cl->call(rlock_protocol::retry, lid, rr);
			VERIFY (ret == rlock_protocol::OK);
			//printf("%s get retry from %s\n", dst.c_str(), client_dst.c_str());
		}
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	}
	else {
  	pthread_mutex_unlock(&mutex);
		return lock_protocol::RETRY;
	}

/*
	if (!lock_client_list[lid].empty()) {
		std::string client_dst = lock_client_list[lid].front();
  	sockaddr_in dstsock;
  	make_sockaddr(client_dst.c_str(), &dstsock);
  	rpcc *cl = new rpcc(dstsock);
  	if (cl->bind() < 0) {
			printf("lock_client: call bind\n");
  	}
		int rr;
		tprintf("sending revoke to %s\n", client_dst.c_str());
		rlock_protocol::status ret = cl->call(rlock_protocol::revoke, lid, rr);
		VERIFY (ret == rlock_protocol::OK);
		tprintf("get revoke from %s\n", client_dst.c_str());
		lock_client_list[lid].erase(lock_client_list[lid].begin());
		delete cl;
	}
	lock_client_list[lid].push_back(dst);

  pthread_mutex_unlock(&mutex);

  lock_protocol::status ret = lock_protocol::OK;
  return ret;
*/
}

lock_protocol::status
lock_server_cache::release(int clt, lock_protocol::lockid_t lid, std::string dst, 
         int &r)
{
  //printf("%s release request of lock %016llx\n", dst.c_str(), lid);
  nacquire --;

  lock_protocol::status ret = lock_protocol::OK;
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server_cache::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}


