
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fstream> 
#include <string>
#include <vector>
#include <queue>
#include <sys/time.h>
#include <mutex>
#include <stdint.h>
#include <string.h>
//using namespace std;

class Node_Topology
{
public:
	int node_id;    //own id
	std::vector<std::vector<bool> > EdgeExist ;  //check if an edge exist 
	std::vector<std::vector<int> >  EdgeCost  ; // the cost of an edge
	std::string log_name ; 						//output log name
	uint32_t	node_timestamp ;				// own timestamp
	std::vector<uint32_t> history ; 			//other node's timestamp history
	char LSP_packet_buffer[1000];				//own LSP buffer
	int  LSP_packet_size;						// own LSP length
	bool rebuild;								// rebuild own graph information when some edge changed
	bool repacket;								//check need to rebuild LSP packet (when only own neighbor change)
	std::mutex repacket_mtx;					// mutex for repacket
	std::mutex graph_mtx;						//mutex for changing info of Edge and rebuild
	std::vector<int> ForwardTable;     		//own forwarding table

	Node_Topology();
	void Node_Topology_init(int id, char* cost_file, std::string log_file ); 
	void Make_LSP_Packet();				
	bool Update_Graph(uint32_t* LSP, int gloabal_Id);
	std::vector< std::pair<int,int> > Find_Neighbor_cost(int id);
	bool Need_Flood(int sender, uint32_t timestamp);
	void Update_EdgeExist(int id1, int id2 , bool exist);
	void Update_EdgeExist_both(int id1, int id2 , bool exist);
	void Update_EdgeCost(int id1, int id2 , int cost);
	void Update_EdgeCost_both(int id1, int id2 , int cost);
	void Monitor_Connection();
	void Connect_Neighbor(short int id);
	void Run_Dijkstra(int own_id);
	bool Check_LSP_Graph(uint32_t* LSP,std::vector< std::pair<int,int> > neighbors);

};
