#include "utils.h"
#include <thread>

using namespace boost::asio;
RoboCupSSLClient client(40102,"224.5.23.2","");
SSL_WrapperPacket recieve_packet;
SSL_DetectionFrame detection;
ip::udp::endpoint remote_endpoint;
boost::system::error_code err;
double x_vel=0.0,y_vel=0.0,w_vel=0.0,ang=0.0,angle=0.0;

pid pid_X(0.002,0.001,0.0),pid_Y(0.0007,0.001,0.0),pid_W(0.03,0.0,0.0);

class DataSend{
private:
	grSim_Packet send_packet;
	grSim_Robot_Command* command;
	std::ostringstream stream;
public:
	DataSend(bool isteamyellow){
		send_packet.mutable_commands()->set_isteamyellow(isteamyellow);
		send_packet.mutable_commands()->set_timestamp(0.0);
		command = send_packet.mutable_commands()->add_robot_commands();
		/*initialize the commands*/
		command->set_id(0);
		command->set_wheelsspeed(false);
		command->set_veltangent(0.0);
		command->set_velnormal(0.0);
		command->set_velangular(0.0);
		command->set_kickspeedx(0.0);
		command->set_kickspeedz(0.0);
		command->set_spinner(false);
		/*Initialization ends*/
	}
	void Stop(int BotID){
		command->set_id(BotID);
		command->set_wheelsspeed(false);
		command->set_veltangent(0.0);
		command->set_velnormal(0.0);
		command->set_velangular(0.0);
		command->set_kickspeedx(0.0);
		command->set_kickspeedz(0.0);
		command->set_spinner(false);		
	}
	void addCommands(int BotID=0,bool SetWheelSpeed=false,double x_vel=0.0,double y_vel=0.0,double w_vel=0.0,double kickSpeedX=0.0,double kickSpeedZ=0.0,bool setSpinner=false){
		command->set_id(BotID);
		command->set_wheelsspeed(SetWheelSpeed);
		command->set_veltangent(x_vel);
		command->set_velnormal(y_vel);
		command->set_velangular(w_vel);
		command->set_kickspeedx(kickSpeedX);
		command->set_kickspeedz(kickSpeedZ);
		command->set_spinner(setSpinner);
	}
	grSim_Robot_Command* getCommands(){
		return command;
	}
	void setCommand(grSim_Robot_Command* new_command){
		command = new_command;
	}
	string Serialize(){
		send_packet.SerializeToOstream(&stream);		
		std::string data = stream.str();
		return data;
	}
};

class Alternate
{
public:
	//Start with kick off from formation for yellow team
	//After kickoff select positions for 2 bots and do an indirect goal
	//Return to previous positions and try kick-off + direct shot
	grSim_Robot_Command* goal(grSim_Robot_Command* command,SSL_DetectionFrame detection){
		SSL_DetectionBall ball = detection.balls(0);
		SSL_DetectionRobot sending_bot = detection.robots_yellow(3),receiving_bot = detection.robots_yellow(2);
		double theta = calc_angle_between_points(-4000.0,0.0,ball.x(),ball.y());
		x_vel = pid_X.calculate(sending_bot.x(),ball.x()+105);
		y_vel = pid_Y.calculate(sending_bot.y(),ball.y()+20);
		ang = calc_angle_between_points(sending_bot.x(),sending_bot.y(),-4000.0,0.0);
		angle = sending_bot.orientation() * 180/PI;
		angle = fmod(angle+450.0,360.0);
		w_vel = pid_W.calculate(ang,angle);
		w_vel = 0.0;
		printf("PidX: %3.2f  PidY: %3.2f  PidW: %3.2f ",x_vel,y_vel,w_vel);
		command->set_id(3);
		command->set_wheelsspeed(false);
		if(angle>=180.0 && angle<=360.0)
			command->set_veltangent(-x_vel); 
		else command->set_veltangent(x_vel);
		if(angle>=180.0 && angle<=360.0)
			command->set_velnormal(-y_vel);
		else command->set_velnormal(y_vel);
		if(abs(w_vel)<=0.50) w_vel = 0.0;
		if(abs(ang - angle) < 180.0)
		command->set_velangular(-w_vel);
		else command->set_velangular(w_vel);
		command->set_kickspeedx(5.0);
		command->set_kickspeedz(0.0);
		command->set_spinner(false);
		return command;		
	}	
};



int main(){
	io_service io_service;
	ip::udp::socket socket(io_service);
	socket.open(ip::udp::v4());
	remote_endpoint = ip::udp::endpoint(ip::address::from_string("127.0.0.1"),20011);	

    //Starting Connection to grSim Multicast Server
    printf("Connecting to Multicast Server......\n");
    client.open(true);
    printf("Connected\n");
    
    //send bot 3 to the center then stop
    bool goal_flag = false;
    while(!goal_flag){
		if (!client.receive(recieve_packet)){
			printf("Connection to Server Unsuccessful!!\n");
			return -1;
		}
		//printf("Client Successfully Connected\n");
		if (!recieve_packet.has_detection()){
			printf("Recieved Packet has no Detection Frame!!\n");
			return -1;
		}
		detection = recieve_packet.detection();
		DataSend messenger(true);//true to say that i am team Yellow
		Alternate a;
		grSim_Robot_Command* cmd = a.goal(messenger.getCommands(),detection);
		messenger.setCommand(cmd);
		std::string data = messenger.Serialize();
		//std::cout<<"str: "<<stream.str()<<"\n";	
		socket.send_to(buffer(data,int(data.length())),remote_endpoint,0,err);
		if(sqrt((detection.balls(0).x()*detection.balls(0).x()) + (detection.balls(0).y()*detection.balls(0).y()))>=20)
			goal_flag = true;		
		if(abs(x_vel)<=0.02 && abs(y_vel)<=0.02)
			goal_flag = true;
		std::cout<<"\n";
	}
	DataSend messenger(true);//true to say that i am team Yellow
	messenger.Stop(3);
	std::string data = messenger.Serialize();
	socket.send_to(buffer(data,int(data.length())),remote_endpoint,0,err);	
	printf("kickoff bot in position\n");
	printf("Done!!\n");
    socket.close();
    return 0;
}