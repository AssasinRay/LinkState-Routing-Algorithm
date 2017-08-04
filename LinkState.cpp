#include "LinkState.h"
#define  node_num  256
#define  debug     1
extern struct timeval globalLastHeartbeat[256];
//extern std::mutex rebuild_mtx;
//extern std::mutex repacket_mtx;
#define lsp_debug 0
#define update_g_debug  0

//initilize all member variables
Node_Topology::Node_Topology(){
	EdgeExist.assign(node_num,std::vector<bool>(node_num,false));
	EdgeCost.assign(node_num,std::vector<int>(node_num,1));
	history.assign(node_num,0);
	ForwardTable.assign(node_num,-1);
	log_name = "";
	rebuild = false;
	repacket = false;
	node_timestamp = 1;
	memset(LSP_packet_buffer, '0', 1000);
	LSP_packet_size= 1;
}

//update the existence of a edge
void  Node_Topology::Update_EdgeExist(int id1, int id2, bool exist){
	EdgeExist[id1][id2] =  exist;
	return;
}

//update the existence of a edge for both
void  Node_Topology::Update_EdgeExist_both(int id1, int id2, bool exist){
	EdgeExist[id1][id2] = EdgeExist[id2][id1] = exist;
	return;
}

//update the cost of a edge
void  Node_Topology::Update_EdgeCost(int id1, int id2, int cost){
	EdgeCost[id1][id2] = cost;
	return;
}

//update the cost of a edge for both
void  Node_Topology::Update_EdgeCost_both(int id1, int id2, int cost){
	EdgeCost[id1][id2] = EdgeCost[id2][id1] = cost;
	return;
}

//set id, edge cost, open log
//me->neighbor
void Node_Topology::Node_Topology_init(int id, char * cost_file, std::string log_file ){
	//assign id 
	node_id = id;
	//open log
	log_name = log_file;
	std::ofstream logfile;
	logfile.open(log_name);
	logfile.close();
	//read cost
	FILE *cost  = fopen(cost_file,"r");
	int id_val;
    int cost_val;
    if(cost==NULL){
    	 std::cout<<"read cost file failed!"<<std::endl;
    	 return;
    }

    while (fscanf(cost, "%d %d\n", &id_val, &cost_val) != EOF){
      		Update_EdgeCost(node_id,id_val,cost_val); 
    }
    fclose(cost);
  	return;
}

//get vaild neighbor (id,cost) pair by  neighbor's id increaseing order
// me->neighbor
std::vector< std::pair<int,int> > Node_Topology::Find_Neighbor_cost(int id){
	std::vector< std::pair<int,int> > res;
	for(int i=0;i<node_num;i++){
		if(i == id) continue;
		if(EdgeExist[id][i] == true){
			res.push_back(std::make_pair(i,EdgeCost[id][i]));
		}
	}
	return res;
}

//connect to a new neighbor hearing from new neighbor   nn->me
void Node_Topology::Connect_Neighbor(short int id){
	if(EdgeExist[node_id][id] == true) return;
	if(debug) std::cout<<node_id<<": connect to "<<id<<std::endl;
	repacket = true;
	rebuild = true;
	Update_EdgeExist_both( node_id, id, true);
	return;
}

//check if it is the latest LSP packet  return false when it is latest
//return true when we have to flood it 
bool Node_Topology::Need_Flood(int sender, uint32_t timestamp){
	if(history[sender] < timestamp){
		history[sender] = timestamp;
		return false;
	}
	//std::cout<<node_id<<"'s timestamp: "<<timestamp<<" is flooded original one is "<<history[sender]<<std::endl;
	return true;
}

//sender LSP packet  
// 0= header 1= nodeid 2= timestamp remain= neighbor's id, cost from biggest id to smallest id 
void Node_Topology::Make_LSP_Packet(){

	std::vector< std::pair<int,int> >  neighbors = Find_Neighbor_cost(node_id);
	int  total_slot = (2*neighbors.size()+4) ;
	LSP_packet_size = total_slot * 4;

	if(lsp_debug) std::cout<<node_id<<": Make_LSP_Packet "<<std::endl;
	uint32_t *packet = (uint32_t *)LSP_packet_buffer;
	packet[0] = (uint32_t)0x12345678;//header
	packet[1] = htonl(node_id); //id 
	packet[2] = htonl(node_timestamp); //time stamp
	node_timestamp++;
	int i =3;
	while(i<total_slot-1){
		uint32_t neighbor_id = neighbors.back().first;
		uint32_t neighbor_cost = neighbors.back().second;
		neighbors.pop_back();
		packet[i++] = htonl(neighbor_id);
		packet[i++] = htonl(neighbor_cost); 
	}
	packet[total_slot-1] = packet[1];
	return ;
}

bool Node_Topology::Check_LSP_Graph(uint32_t* LSP,std::vector< std::pair<int,int> > neighbors){
	uint32_t sender = ntohl(LSP[1]);
	uint32_t new_neighbor_id, old_neighbor_id,new_neighbor_cost,old_neighbor_cost;
	int i=3;
	while(ntohl(LSP[i]) != sender){
		new_neighbor_id 		= ntohl(LSP[i++]);
		new_neighbor_cost 		= ntohl(LSP[i++]);
		if(neighbors.empty())  return true;
		old_neighbor_id  		= neighbors.back().first;	
		old_neighbor_cost 		= neighbors.back().second;
		neighbors.pop_back();
		if(new_neighbor_cost!=old_neighbor_cost or new_neighbor_id != old_neighbor_id) return true;
	}
	if(neighbors.empty() == false) return true;
	//std::cout<<"no need"<<std::endl;
	return false;
}

//receive other's LSP packet and update own knowledge
//if updated return true else false
bool Node_Topology::Update_Graph(uint32_t* LSP,int gloabal_Id){
	uint32_t sender = ntohl(LSP[1]);
	uint32_t sender_timestamp = ntohl(LSP[2]);

	//check outdated LSP packet
	if(Need_Flood(sender, sender_timestamp) == true or gloabal_Id == sender) return false;

	if(update_g_debug) std::cout<<node_id<<": Update_Graph from node"<<sender<<std::endl;

	//read information from biggest node id to smallest
	std::vector< std::pair<int,int> > neighbors = Find_Neighbor_cost(sender);
	uint32_t new_neighbor_id, old_neighbor_id,new_neighbor_cost,old_neighbor_cost;
	int i = 3;

	//if(Check_LSP_Graph(LSP,neighbors) == false) return false;

	//delete old connection
	while(neighbors.empty()==false){
		Update_EdgeExist_both(sender,neighbors.back().first, false);
		neighbors.pop_back();
	}

	//add new connections
	while(ntohl(LSP[i]) != sender){
		new_neighbor_id 		= ntohl(LSP[i++]);
		new_neighbor_cost 		= ntohl(LSP[i++]);
		Update_EdgeExist_both(sender, new_neighbor_id,  true);
		Update_EdgeCost(sender,  new_neighbor_id,  new_neighbor_cost);		
	}

	rebuild = true;

	return true;
}


//monitor neighbors lose connection in another thread
//sending packet to neighbors
void Node_Topology::Monitor_Connection(){
	std::vector< std::pair<int,int> > neighbors = Find_Neighbor_cost(node_id);
	struct timeval now;
	gettimeofday(&now, 0);
	for(int i=0;i<neighbors.size();i++){
		int neighbor_id = neighbors[i].first;
		if( now.tv_sec - globalLastHeartbeat[neighbor_id].tv_sec > 1){
			rebuild = true;
			Update_EdgeExist_both(node_id ,neighbor_id, false);
			if(debug) std::cout<<node_id<<": connection down of "<<neighbor_id<<std::endl;
			repacket = true;
		}
	}
	return;
}

//using Dijkstra to update forward table
void Node_Topology::Run_Dijkstra(int own_id){
	 // initilization
	//compare function pair<id,distance> if distance is equal then use id 
	struct compare_function{
	   bool operator()(const std::pair<int,int> a, const std::pair<int,int> b){
	   	return (a.second > 	b.second 	or 
	   			(a.second == b.second 	and a.first > b.first));
	   }
	 };
	 std::priority_queue<std::pair<int,int> ,std::vector<std::pair<int,int> >, compare_function> tentative;
	 std::vector<bool> 	confirmed(node_num,false);
	 std::vector<int> 	distance(node_num, 999999);
	 std::vector<int> 	previous(node_num,-1);
	 ForwardTable.assign(node_num,-1);

	 previous[own_id] 	  = -1;
	 tentative.push(std::make_pair(own_id,0));
	 distance[own_id]  = 0;

	 while( tentative.empty() == false){
	 	  int cur_id 	= 	tentative.top().first;
	 	  int cur_cost 	=	tentative.top().second;
	 	  tentative.pop();
	 	  if(confirmed[cur_id] == true) continue;
	 	  confirmed[cur_id] = true;
	 	  std::vector< std::pair<int,int> > neighbors = Find_Neighbor_cost(cur_id);

	 	  for(int i =0;i<neighbors.size();i++){
	 	  	 int neighbor_id 	= 	neighbors[i].first;
	 	  	 int neighbor_cost 	= 	neighbors[i].second;
	 	  	 if(confirmed[neighbor_id] == true) continue;

	 	  	 int new_distance = distance[cur_id] + neighbor_cost;
	 	  	 int old_distance = distance[neighbor_id];
	 	  	 if( old_distance > new_distance or 
	 	  	 	(old_distance == new_distance and previous[neighbor_id] > cur_id)){
	 	  	 	distance[neighbor_id] = new_distance;
	 	  	 	previous[neighbor_id] = cur_id;
	 	  	 	tentative.push(std::make_pair(neighbor_id,new_distance));
	 	  	 }
	 	  }
	 }

	 //update ForwardTable
	 ForwardTable[own_id] = own_id;
	 for(int i=0;i<node_num;i++){
	 	 int previous_pos = i;
	 	 int temp 		  = i;
	 	 while(temp != own_id and temp != -1){
	 	 	previous_pos = temp;
	 	 	temp = previous[temp];
	 	 }
	 	 if(temp!=  -1){
	 	 	//cout<<previous_Pos;
	 	 	int tmp = temp;
	 	 	ForwardTable[i] = previous_pos;
	 	 }
	 	 else{
	 	 	int tmp = temp;
	 	 	//cout<<tmp;
	 	 	ForwardTable[i] = -1;
	 	 }
	 }
	 return;

}
/*
int main()
{
	//Node_Topology f;
	//char* p0 = "./example_topology/test2initcosts0";
	// char* p1 = "./example_topology/test2initcosts1";
	// char* p2 = "./example_topology/test2initcosts2";
	// char* p3 = "./example_topology/test2initcosts3";

	//f.Node_Topology_init(1,p0, "cao" );
	//f.Node_Topology_init(2,p1, "ni");
	//f.Node_Topology_init(3,p2, "ma" );
	//f.Node_Topology_init(4,p3, "ma");

	//f.Update_EdgeExist(255,1,true);
	//std::vector< std::pair<int,int> > a= f.Find_Neighbor_cost(255);
	//std::cout<<a[0].first<<"   "<<a[0].second<<std::endl;
	//std::vector< std::pair<int,int> > b= f.Find_Neighbor_cost(1);

	//f.Make_LSP_Packet();
	//printf("%s\n", f.LSP_packet_buffer);
	//std::cout<<"lsp de size"<<f.LSP_packet_size<<std::endl;
	//bool flag = (0x4c535000==('L' << 24) | ('S' << 16) | ('P' << 8));
	//std::cout<<flag<<std::endl;

	Node_Topology a;
	Node_Topology b;
	char* p0 = "./example_topology/mytest0";
	char* p1 = "./example_topology/mytest0";
	a.Node_Topology_init(1,p0, "cao" );
	b.Node_Topology_init(2,p1, "ni");

	a.Update_EdgeCost(1,3,3);
	a.Update_EdgeCost(1,4,4);
	a.Update_EdgeCost(1,5,5);
	a.Update_EdgeCost(1,6,6);
	a.Update_EdgeCost(1,7,7);
	a.Update_EdgeCost(1,8,8);
	a.Update_EdgeCost(2,7,2);
	a.Update_EdgeCost(2,8,2);

	a.Update_EdgeExist(1,3,true);
	a.Update_EdgeExist(1,4,true);
	a.Update_EdgeExist(1,5,false);
	a.Update_EdgeExist(1,6,false);
	//a.Update_EdgeExist(1,7,true);
	a.Update_EdgeExist(1,8,true);
	a.Update_EdgeExist(2,7,true);
	a.Update_EdgeExist(2,8,true);
	a.Update_EdgeExist(2,3,true);

	//a.Update_EdgeCost(1,3,3);
	//a.Update_EdgeExist(1,3,true);
	a.Make_LSP_Packet();

	b.Update_EdgeCost(2,3,3);
	b.Update_EdgeCost(2,4,4);
	b.Update_EdgeCost(2,5,5);
	b.Update_EdgeCost(2,6,6);
	b.Update_EdgeCost(2,7,7);
	b.Update_EdgeCost(2,8,8);
	b.Update_EdgeCost(1,5,5);

	b.Update_EdgeExist(1,5,true);
	b.Update_EdgeExist(2,3,false);
	b.Update_EdgeExist(2,4,false);
	b.Update_EdgeExist(2,5,true);
	b.Update_EdgeExist(2,6,true);
	b.Update_EdgeExist(2,7,true);
	b.Update_EdgeExist(2,8,false);
	//b.Update_EdgeCost(2,4,4);
	//b.Update_EdgeExist(2,4,true);
	b.Make_LSP_Packet();

	a.Update_Graph((uint32_t *)b.LSP_packet_buffer,1);
	b.Update_Graph((uint32_t *)a.LSP_packet_buffer,2);

	// for(int i=0;i<node_num;i++)
	// {
	// 	for(int j=0;j<node_num;j++)
	// 	{
	// 		if(a.EdgeExist[i][j]){
	// 			std::cout<<"a's i: "<<i<<std::endl;
	// 			std::cout<<"a's j: "<<j<<std::endl;
	// 			std::cout<<"a's ij cost: "<<a.EdgeCost[i][j]<<std::endl;
	// 		}

	// 	}
	// }

	// for(int i=0;i<node_num;i++)
	// {
	// 	for(int j=0;j<node_num;j++)
	// 	{
	// 		if(b.EdgeExist[i][j]){
	// 			std::cout<<"b's i: "<<i<<std::endl;
	// 			std::cout<<"b's j: "<<j<<std::endl;
	// 			std::cout<<"b's ij cost: "<<b.EdgeCost[i][j]<<std::endl;
	// 		}

	// 	}
	// }

	a.Run_Dijkstra(1);
	b.Run_Dijkstra(2);

	for(int i=0;i<node_num;i++)
	{
			if(a.ForwardTable[i] !=-1){
				std::cout<<"a's i: "<<i<<std::endl;
				//std::cout<<"a's j: "<<j<<std::endl;
				std::cout<<"a's forward: "<<a.ForwardTable[i]<<std::endl;
			}
	}

		for(int i=0;i<node_num;i++)
		{
			if(b.ForwardTable[i] !=-1){
				std::cout<<"b's i: "<<i<<std::endl;
				//std::cout<<"b's j: "<<j<<std::endl;
				std::cout<<"b's forward: "<<b.ForwardTable[i]<<std::endl;
			}


		}
	

	//manual settings



	return 0;
}*/
/*
int main(){

	Node_Topology a;
	Node_Topology b;
	Node_Topology c;
	Node_Topology d;
	char* p0 = "./example_topology/mytest0";
	a.Node_Topology_init(1,p0, "cao" );
	b.Node_Topology_init(2,p0, "ni");
	c.Node_Topology_init(3,p0, "ma");
	d.Node_Topology_init(4,p0, "a");

	a.Update_EdgeExist(1,3,true);
	a.Update_EdgeExist(1,2,true);
	a.Update_EdgeCost(1,3, 10);
	a.Update_EdgeCost(1,2,5);

	b.Update_EdgeExist(2,3,true);
	b.Update_EdgeExist(2,1,true);
	b.Update_EdgeExist(2,4,true);
	b.Update_EdgeCost(2,3, 3);
	b.Update_EdgeCost(4,2,11);
	b.Update_EdgeCost(2,1,5);

	c.Update_EdgeExist(2,3,true);
	c.Update_EdgeExist(3,4,true);
	c.Update_EdgeExist(3,1,true);
	c.Update_EdgeCost(2,3, 3);
	c.Update_EdgeCost(3,4,2);
	c.Update_EdgeCost(3,1,10);

	d.Update_EdgeExist(4,3,true);
	d.Update_EdgeExist(4,2,true);
	d.Update_EdgeCost(4,3, 2);
	d.Update_EdgeCost(4,2,11);

	for(int s=0;s<2;s++){
		a.Make_LSP_Packet();
		b.Make_LSP_Packet();
		c.Make_LSP_Packet();
		d.Make_LSP_Packet();

		a.Update_Graph((uint32_t *)b.LSP_packet_buffer,1);
		a.Update_Graph((uint32_t *)c.LSP_packet_buffer,1);
		a.Update_Graph((uint32_t *)d.LSP_packet_buffer,1);

		b.Update_Graph((uint32_t *)a.LSP_packet_buffer,2);
		b.Update_Graph((uint32_t *)c.LSP_packet_buffer,2);
		b.Update_Graph((uint32_t *)d.LSP_packet_buffer,2);

		c.Update_Graph((uint32_t *)b.LSP_packet_buffer,3);
		c.Update_Graph((uint32_t *)a.LSP_packet_buffer,3);
		c.Update_Graph((uint32_t *)d.LSP_packet_buffer,3);

		d.Update_Graph((uint32_t *)b.LSP_packet_buffer,4);
		d.Update_Graph((uint32_t *)c.LSP_packet_buffer,4);
		d.Update_Graph((uint32_t *)a.LSP_packet_buffer,4);
	}	

	a.Run_Dijkstra(1);
	b.Run_Dijkstra(2);
	c.Run_Dijkstra(3);
	d.Run_Dijkstra(4);


	for(int i=0;i<node_num;i++)
	{
			if(a.ForwardTable[i] !=-1){
				std::cout<<"a's i: "<<i<<std::endl;
				//std::cout<<"a's j: "<<j<<std::endl;
				std::cout<<"a's forward: "<<a.ForwardTable[i]<<std::endl;
			}
	}

		for(int i=0;i<node_num;i++)
		{
			if(b.ForwardTable[i] !=-1){
				std::cout<<"b's i: "<<i<<std::endl;
				//std::cout<<"b's j: "<<j<<std::endl;
				std::cout<<"b's forward: "<<b.ForwardTable[i]<<std::endl;
			}


		}

				for(int i=0;i<node_num;i++)
		{
			if(c.ForwardTable[i] !=-1){
				std::cout<<"c's i: "<<i<<std::endl;
				//std::cout<<"b's j: "<<j<<std::endl;
				std::cout<<"c's forward: "<<c.ForwardTable[i]<<std::endl;
			}


		}

				for(int i=0;i<node_num;i++)
		{
			if(d.ForwardTable[i] !=-1){
				std::cout<<"d's i: "<<i<<std::endl;
				//std::cout<<"b's j: "<<j<<std::endl;
				std::cout<<"d's forward: "<<d.ForwardTable[i]<<std::endl;
			}


		}
}*/
