#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include <mutex> // 引入互斥锁头文件
#include<string>
#include<thread>
#include <unordered_map>
#include <vector>
#include <map>
#include <set>
#pragma comment (lib,"ws2_32.lib")
using namespace std;
sockaddr_in serverAddr;
sockaddr_in clientAddr;
SOCKET  serverSock, clientSock;
int clientAddrsizef = sizeof(clientAddr);
bool pan = false;
int clientCount = 0; // 客户端数量计数器
std::vector<SOCKET> clientSocks; // 存储所有客户端套接字的向量
std::mutex mtx;

void accept();//负责连接客户端
void allocation();//负责配置socket的ip和端口
void SendThread(SOCKET clientSock);//负责客户端发送的的线程
void xiancient(SOCKET clientSock, sockaddr_in  clientAddr);//负责处理客户端接收信息，并处理群聊和私聊的线程

int main(){
	allocation();
	while (!pan) { 
		accept();
		thread  clientThread(xiancient,clientSock, clientAddr);
		thread sendThread(SendThread, clientSock);
		// 分离线程，让线程自主运行
		clientThread.detach();
		sendThread.detach();
		std::cout << "输入esc退出："<<"query查询" << endl;
	}
	closesocket(serverSock);
	WSACleanup();
	return 0;
}

void xiancient(SOCKET clientSock, sockaddr_in  clientAddr) {//负责处理客户端接收信息，并处理群聊和私聊的线程
	string shuru;
	char clientbuf[4096] ;//接收
	int bytesReceived = 0;
	char clientIP[INET_ADDRSTRLEN];
	string clientIP_1[10];//ip数组
	int clientPort_1[10];//端口数组
	inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
	
	unsigned short clientPort = ntohs(clientAddr.sin_port);
	{
		std::cout << "客户端 " << clientIP << ":" << clientPort << " 已连接" << endl;
		std::lock_guard<std::mutex> guard(mtx); // 在访问共享资源前锁定互斥锁
		clientIP_1[clientCount] = clientIP;
		clientPort_1[clientCount] = clientPort;
	}
	cout << ++clientCount << endl;
	while (!pan) {
		memset(clientbuf, 0, sizeof(clientbuf));// 接收客户端消息
		bytesReceived = recv(clientSock, clientbuf, sizeof(clientbuf), 0);
		if (bytesReceived > 0) {
			 //假设消息格式为 "P|receiverId|message" 或 "G|groupId|message"
			std::string msg(clientbuf, bytesReceived);
			if (msg[0] == 'P') { // 私聊消息
				// 解析消息并找到目标接收者
				char shibuf[1024] = "进入私聊";
				send(clientSock, shibuf, sizeof(shibuf), 0);

				size_t separatorPos = msg.find('|', 1);
				if (separatorPos != std::string::npos) {
					std::string targetClientIdStr = msg.substr(1, separatorPos - 1);
					std::string actualMessage = msg.substr(separatorPos + 1);
					int targetClientId = std::stoi(targetClientIdStr) - 1; // 转换为0-based索引
					if (targetClientId >= 0 && targetClientId < clientSocks.size()) {
						// 找到目标客户端套接字
						SOCKET targetSock = clientSocks[targetClientId];
						// 向目标客户端发送实际消息
						send(targetSock, actualMessage.c_str(), actualMessage.length(), 0);
						std::cout << "消息已发送至客户端ID " << targetClientId + 1 << std::endl;
					}else {
						std::cout << "无效的客户端ID: " << targetClientId + 1 << std::endl;
					}
				}else {
					std::cout << "无效的私聊消息格式。" << std::endl;
				}	
			}else if (msg[0] == 'G') { // 群聊消息
				char qunbuf[1024] = "进入群聊";
				send(clientSock, qunbuf, sizeof(qunbuf), 0);
				memset(clientbuf, 0, sizeof(clientbuf));// 接收客户端消息
				std::cout << clientIP << ":" << clientPort << " 发送群聊消息：" << msg.substr(1) << endl;
				clientSocks.push_back(clientSock); // 将新客户端套接字加入列表
				auto messageContent = msg.substr(1);
				
				for (SOCKET& c : clientSocks) {
					send(c, msg.substr(1).c_str(), msg.size() - 1, 0);
				}
			}else if(msg[0]!=('G' || 'P')) {
				std::cout << clientIP << ":" << clientPort << " 接收到信息：" << clientbuf << endl;
				send(clientSock, clientbuf, sizeof(clientbuf), 0);
			}
		}else if (bytesReceived == 0) {
			std::cout << clientIP << ":" << clientPort << " 客户端断开连接：" << endl;
			{
				std::lock_guard<std::mutex> guard(mtx); // 在修改共享资源前锁定互斥锁
				cout << --clientCount << endl;// 每当有客户端断开连接时，减少计数器
			}
			break;
		}else {
			std::cout << "接收时出错：" << endl;
			{
				std::lock_guard<std::mutex> guard(mtx); // 在修改共享资源前锁定互斥锁
				cout << --clientCount << endl;
			}
			break;
		}
	}
	closesocket(clientSock);
}


void allocation() {//负责配置socket的ip和端口
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "初始化socket库失败！" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "初始化socket库成功！" << endl;
	}
	if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		std::cout << "套接socket失败" << endl;
		WSACleanup();
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cout << "bind失败" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "绑定成功！" << endl;
	}
	if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "监听失败" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "监听成功！" << endl;
	}
}

void SendThread(SOCKET clientSock) {//负责客户端发送的的线程
	clientSocks.push_back(clientSock); // 将新客户端套接字加入列表	
	while (!pan) {
		string shur;
		cin >> shur;
		if (shur == "esc") {
			cout << "强行退出" << clientCount << endl;
			pan = true;
			//closesocket(serverSock);
			exit(0);
			break;
		}
		if (shur == "query") {
			lock_guard<std::mutex> lock(mtx);
			cout << "当前客户端数量：" << clientCount << endl;
		} 
			for (SOCKET& c : clientSocks) {
				send(c, shur.substr().c_str(), shur.size(), 0);
			}
		}
	}


void accept() {//负责连接客户端
	clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientAddrsizef);
	if (clientSock == INVALID_SOCKET) {
		std::cout << "未能接收客户端连接" << endl;
		closesocket(serverSock);
		WSACleanup();
	}else {
		std::cout << "接收客户端连接成功！" << endl;
	}
}

