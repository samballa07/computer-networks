import subprocess
import time

assignment_dir="./assignment1"
server_executable="${assignment_dir}/server"
client_executable="${assignment_dir}/client"
drop_rate = 20
f = open("server_output.txt", "w")
print("Running test..")

subprocess.run("docker run -t -d --name=udp_test -v$(pwd):/opt baseline:latest", shell=True)

com = "docker exec udp_test /opt/assignment1/server -p 41717"
com = com.split()
print(com)
subprocess.run("sleep 1", shell=True)

subprocess.Popen(com, stdout=f)

client1 = open("client1_out.txt", "w")
client_com = "docker exec -ti udp_test /opt/assignment1/client -a 127.0.0.1 -p 41717 -n 10000 -t 5".split()

subprocess.Popen(client_com, stdout=client1)

time.sleep(4)

subprocess.run("docker kill udp_test", shell=True)
subprocess.run("docker rm udp_test", shell=True)


