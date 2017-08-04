#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream> 


using namespace std;

string Send_log(int dest, int nexthop, char* msg)
{
	string message(msg);
	string text = "sending packet dest "+to_string(dest)+" nexthop "+to_string(nexthop)+" message "+message + "\n";
	return text;

}

string Receive_log(char* msg)
{
	string message(msg);
	string text = "receive packet message "+message+ "\n";
	return text;
}

string Forward_log(int dest, int nexthop, char* msg)
{
	string message(msg);
	string text = "forward packet dest "+to_string(dest)+" nexthop "+to_string(nexthop)+" message "+message+ "\n";
	return text;
}

string Unreach_log(int dest)
{
	string text = "unreachable dest "+to_string(dest)+ "\n";
	return text;
}
/*
int main()
{
	char nothing = (char)0xAA;    //no update 
	char* lsp_packet = &nothing;
	cout<<sizeof(lsp_packet)<<endl;
	cout<<strlen(lsp_packet)<<endl;

	char* l = "asdasdasdasdasdasdasdasdasdasdasdasd";
	cout<<sizeof(l)<<endl;
	cout<<strlen(l)<<endl;
	return 0;
}

*/