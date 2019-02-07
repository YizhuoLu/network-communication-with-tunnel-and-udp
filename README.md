# network-communication-with-tunnel-and-udp

Language: C  Environment: Linux

There are two stages for this project:

First stage: starting processes

  A parent processs (act as primary router) will fork a child process (act as the second router). Then these two routers will communicate via UDP connection. Say that primary router will get the pid of second router throught udp, etc. Then through typing command to pass configuration file to the main function, these two routers will start and know it's stage 1 in which their job is to print log file at the current folder which is just like below:
  
  
  In stage1.r0.out:
  
  primary port: 44051
  
  router: 1, pid: 3104, port: 43031
  
  router 0 closed
  
  
  In stage1.r1.out:
  
  router: 1, pid: 3104, port: 43031
  
  router 1 closed
  
  
configuration file is like:

  #This is a comment line, should be ignored
  
  stage 1
  
  # There can be other comments, or variable numbers of spaces or tabs in lines.
  
  # Clarification 2019-01-21: 1 router means one secondary router,
  
  # in addition to the implicit primary router.
  
  num_routers   1
  
  
  Attention: I used xx.sin_port = htons(0) && getsockname() to dynamically get the port number for primary router. so each time u run my program, it will be different.
  
  
Second stage: ping through tunnel
  
  
  At this stage, I will set up a tunnel through two commend:
  
  
  sudo ip tuntap add dev tun1 mode tun
  
  
  sudo ifconfig tun1 10.5.51.2/24 up
  
  
  Then I will type commend: sudo ./proja config_file_2 to start the program, right after that (since I have set 15s timeout in select() whihc is waiting from either tunnel or UDP) I will send four packets to 10.5.51.x say 10.5.51.3 (since I have used the second commend above to bind 10.5.51/24 block ip with the another end of the tunnel) to ping in another terminal. After that, primary will read from tunnel to get those four ICMP echo request packets (since it's ping). Then primary router will print IP src address, IP dst address, ICMP type in stage2.r0.log file. Afterwards, primary router will forward the packet to second router via UDP connection. Second router will open it change the checksum of ICMP packets, src and dst IP addresses, ICMP type to convert it into a ICMP echo reply packet. It will also print relatetive info in the stage2.r1.log file. Then second router will forward it back to primary router. Eventually, primary router will write it back to user through tunnel.
   
   The stage 2 configuration file and two routers' log files are just like below:
   
   configuration file:
   
   #This is a comment line, should be ignored
   
   stage 2
   
   num_routers 1
   
  
  stage2.r0.log:
  
  primary port: 44051
  
  router: 1, pid: 3104, port: 43031
  
  ICMP from tunnel, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 43031, src: 10.5.51.3, dst: 10.5.51.2, type: 0
  
  ICMP from tunnel, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 43031, src: 10.5.51.3, dst: 10.5.51.2, type: 0
  
  ICMP from tunnel, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 43031, src: 10.5.51.3, dst: 10.5.51.2, type: 0
  
  ICMP from tunnel, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 43031, src: 10.5.51.3, dst: 10.5.51.2, type: 0
  
  router 1 closed
  
  

  In stage2.r1.out:
  
  router: 1, pid: 3104, port: 43031
  
  ICMP from port: 44051, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 44051, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 44051, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  ICMP from port: 44051, src: 10.5.51.2, dst: 10.5.51.3, type: 8
  
  router 1 closed
 
