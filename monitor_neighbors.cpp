#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "LinkState.h"


extern int globalMyID;
#define debug  1
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];
extern class Node_Topology::Node_Topology Toplogy;
//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.

//std::mutex rebuild_mtx;
//std::mutex repacket_mtx;


std::string Send_log(int dest, int nexthop, char* msg)
{
	if(debug==0) return "";
	std::string message(msg);
	std::string text = "sending packet dest "+std::to_string(dest)+" nexthop "+std::to_string(nexthop)+" message "+message + "\n";
	std::cout<<globalMyID<<": "<<text<<std::endl;
	return text;

}

std::string Receive_log(char* msg)
{
	if(debug==0) return "";
	std::string message(msg);
	std::string text = "receive packet message "+message+ "\n";
std::cout<<globalMyID<<": "<<text<<std::endl;
	return text;
}

std::string Forward_log(int dest, int nexthop, char* msg)
{
	if(debug==0) return "";
	std::string message(msg);
	std::string text = "forward packet dest "+std::to_string(dest)+" nexthop "+std::to_string(nexthop)+" message "+message+ "\n";
std::cout<<globalMyID<<": "<<text<<std::endl;
	return text;
}

std::string Unreach_log(int dest)
{
	if(debug==0) return "";
	std::string text = "unreachable dest "+std::to_string(dest)+ "\n";
std::cout<<globalMyID<<": "<<text<<std::endl;

	return text;
}

void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID){ //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));}
}

void hackyBroadcast_LSP(const char* buf, int length)
{
	struct timespec sleepFor;
	Toplogy.graph_mtx.lock();
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID and Toplogy.EdgeExist[globalMyID][i]){ //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));}
	Toplogy.graph_mtx.unlock();
}
void* BroadCastHeartBeat(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	char nothing = (unsigned char)0xAA;    //no update 
	char* lsp_packet = &nothing;
	while(1)
	{
		hackyBroadcast(lsp_packet, 1);
		nanosleep(&sleepFor, 0);
	}
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	char nothing = (unsigned char)0xAA;    //no update 
	char* lsp_packet = &nothing;
	bool  update = false;
	//constantly send LSP packet to neighbors
	
	struct timespec wait;
	wait.tv_sec = 0;
	wait.tv_nsec = 250 * 1000 * 1000; //300 ms
	while(1)
	{
		lsp_packet = &nothing;
		//hackyBroadcast(lsp_packet, 1);
		hackyBroadcast(lsp_packet, 1);
		Toplogy.graph_mtx.lock();
		Toplogy.repacket_mtx.lock();

		Toplogy.Monitor_Connection();
		if(Toplogy.repacket == true){
			update = true;
		}

		Toplogy.graph_mtx.unlock();
		Toplogy.repacket_mtx.unlock();


		if(update){
			nanosleep(&wait, 0);
			Toplogy.graph_mtx.lock();
			Toplogy.repacket_mtx.lock();

			Toplogy.repacket = false;
			Toplogy.Make_LSP_Packet();
			lsp_packet = &Toplogy.LSP_packet_buffer[0];
			
			Toplogy.repacket_mtx.unlock();
			Toplogy.graph_mtx.unlock();

			hackyBroadcast_LSP(lsp_packet, Toplogy.LSP_packet_size);
			//hackyBroadcast(lsp_packet, Toplogy.LSP_packet_size);
		}
		update = false;
		nanosleep(&sleepFor, 0);
	}
}
//std::ios_base:app
void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];
	memset(recvBuf, '\0', 1000);
	FILE* logfile;
	char logLine[256];
	memset(logLine, '\0', 256);

	int bytesRecvd;
	while(1)
	{
		
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		recvBuf[bytesRecvd] = '\0';

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
				Toplogy.graph_mtx.lock(); 
				Toplogy.repacket_mtx.lock();
				Toplogy.Connect_Neighbor(heardFrom);
				Toplogy.repacket_mtx.unlock();
				Toplogy.graph_mtx.unlock();
			
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
			//if(globalMyID == 0) std::cout<<globalLastHeartbeat[1].tv_usec<<std::endl;
		}

		//regular no update LSP packet
		if(recvBuf[0] == 0xAA){
			 int nochange ;
			 int make_send;
			 continue;
			 //gettimeofday(&globalLastHeartbeat[heardFrom], 0);
			
		}

		//noticed to know I am the dest node
		else if(!strncmp((const char*)recvBuf, "recv", 4))
		{
				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "receive packet message %s\n",message );
				fwrite(logLine, 1, strlen(logLine),logfile);
				Receive_log(message);
				fclose(logfile);
		}
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		else if(!strncmp((const char*)recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node

			//change header name
			((char*)recvBuf)[0] = 'f';
			((char*)recvBuf)[1] = 'o';
			((char*)recvBuf)[2] = 'w';
			((char*)recvBuf)[3] = 'd';

			//get dest id
			uint16_t dest_id = ntohs(((const uint16_t*)recvBuf)[2]);
			int      temp    = dest_id;
			temp--;
			Toplogy.graph_mtx.lock();
			Toplogy.Run_Dijkstra(globalMyID);
			int      next_pos = Toplogy.ForwardTable[dest_id];
			Toplogy.graph_mtx.unlock();	

			if(next_pos == globalMyID){
				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "receive packet message %s\n",message );
				fwrite(logLine, 1, strlen(logLine),logfile);
				Receive_log(message);
				fclose(logfile);
			}
			//check next pos is dest node2	2
			else if(next_pos == dest_id)
			{
				((char*)recvBuf)[0] = 'r';
				((char*)recvBuf)[1] = 'e';
				((char*)recvBuf)[2] = 'c';
				((char*)recvBuf)[3] = 'v';

				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "sending packet dest %d nexthop %d message %s\n", dest_id, next_pos,message);
				fwrite(logLine, 1, strlen(logLine),logfile);
				Send_log(dest_id, next_pos,message);
				fclose(logfile);

				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
				      (struct sockaddr*)&globalNodeAddrs[next_pos], sizeof(globalNodeAddrs[next_pos]));
			}
			//forward packet
			else if(next_pos != -1){

				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "sending packet dest %d nexthop %d message %s\n", dest_id, next_pos,message);
				fwrite(logLine, 1, strlen(logLine),logfile);
				Send_log(dest_id, next_pos,message);
				fclose(logfile);

				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
				      (struct sockaddr*)&globalNodeAddrs[next_pos], sizeof(globalNodeAddrs[next_pos]));
			}
			//unreachable
			else if(next_pos == -1){

				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "unreachable dest %d\n", dest_id);	
				fwrite(logLine, 1, strlen(logLine),logfile);
				Unreach_log(dest_id);
				fclose(logfile);
			}

		}

		else if(!strncmp((const char*)recvBuf, "fowd", 4))
		{
			uint16_t dest_id = ntohs(((const uint16_t*)recvBuf)[2]);
			int      temp    = dest_id;
			temp--;
			Toplogy.graph_mtx.lock();
			Toplogy.Run_Dijkstra(globalMyID);
			int      next_pos = Toplogy.ForwardTable[dest_id];
			Toplogy.graph_mtx.unlock();

			//check next pos is dest node
			if(next_pos == globalMyID){
				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "receive packet message %s\n",message );
				fwrite(logLine, 1, strlen(logLine),logfile);
				Receive_log(message);
				fclose(logfile);
			}
			else if(next_pos == dest_id)
			{
				((char*)recvBuf)[0] = 'r';
				((char*)recvBuf)[1] = 'e';
				((char*)recvBuf)[2] = 'c';
				((char*)recvBuf)[3] = 'v';

				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", dest_id, next_pos, message);
				fwrite(logLine, 1, strlen(logLine),logfile);
				Forward_log( dest_id, next_pos,message);
				fclose(logfile);

				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
				      (struct sockaddr*)&globalNodeAddrs[next_pos], sizeof(globalNodeAddrs[next_pos]));
			}
			//forward packet
			else if(next_pos != -1){

				char* message = &((char*)recvBuf)[6];
				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", dest_id, next_pos,message);
				fwrite(logLine, 1, strlen(logLine),logfile);
				Forward_log( dest_id, next_pos,message);
				fclose(logfile);

				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
				      (struct sockaddr*)&globalNodeAddrs[next_pos], sizeof(globalNodeAddrs[next_pos]));
			}
			//unreachable
			else if(next_pos == -1){

				logfile = fopen(Toplogy.log_name.c_str(),"a");
				sprintf(logLine, "unreachable dest %d\n", dest_id);	
				Unreach_log(dest_id);
				fwrite(logLine, 1, strlen(logLine),logfile);
				fclose(logfile);

			}
		}

		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp((const char*)recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			uint16_t neighbor_id = ntohs(((const uint16_t*)recvBuf)[2]);
			int      temp    = neighbor_id;
			temp--;
			void *new_cost_pos = &((char *)recvBuf)[6];
			temp++;
			uint32_t new_cost = ntohl(((uint32_t *)new_cost_pos)[0]);
			temp = new_cost;

			Toplogy.graph_mtx.lock();
			if(Toplogy.EdgeCost[globalMyID][neighbor_id] != new_cost){
				Toplogy.Update_EdgeCost(globalMyID, neighbor_id,new_cost);
				if(Toplogy.EdgeExist[globalMyID][neighbor_id] == true){
					Toplogy.rebuild = true;
					Toplogy.repacket_mtx.lock();
					//std::cout<<"change repacket in cost packet"<<std::endl;
					Toplogy.repacket = true;
					Toplogy.repacket_mtx.unlock();
				}
			}
			Toplogy.graph_mtx.unlock();
		}
		
		//LSP packet  0x12345678  sender_id sender_time info with its neighbor id neighbor cost
		else if(((uint32_t *)recvBuf)[0] == 0x12345678)
		{
			uint32_t *recvIntBuf = (uint32_t *)recvBuf;

			//check we need to update graph
			Toplogy.graph_mtx.lock();
			bool update_graph= Toplogy.Update_Graph(recvIntBuf, globalMyID);
			Toplogy.graph_mtx.unlock();

			if(update_graph == true){
				//forward their lsp parckets
				hackyBroadcast_LSP((const char*)recvBuf, bytesRecvd);
				//hackyBroadcast((const char*)recvBuf, bytesRecvd);
				}

			
		}

		//check need to reconstruct forward table
		/*Toplogy.graph_mtx.lock();
		if(Toplogy.rebuild == true){
			Toplogy.rebuild = false;
			Toplogy.Run_Dijkstra(globalMyID);
		}
		Toplogy.graph_mtx.unlock();*/
		//gettimeofday(&globalLastHeartbeat[heardFrom], 0);
	}

	//(should never reach here)
	close(globalSocketUDP);
}

