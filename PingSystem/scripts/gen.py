import json
commond_num = 0

def upload_config(config_file, bash_script):
    with open(config_file, 'r') as file:
        config_data = json.load(file)

    with open(bash_script, 'w') as bash_script:
        for host, ip in config_data['nat'].items():
            key_filename = f"keys/{ip}.pem"
            bash_script.write(f"scp -r -i {key_filename} ../config azureuser@{ip}:/home/azureuser/\n")

def upload_build(config_file, bash_script):
    with open(config_file, 'r') as file:
        config_data = json.load(file)

    with open(bash_script, 'a') as bash_script:
        for ip in config_data['nat'].values():
            key_filename = f"keys/{ip}.pem"
            bash_script.write(f"scp -r -i {key_filename} ../build azureuser@{ip}:/home/azureuser/\n")

def generate_bnode_command(index, host, usernum, grpcnum, paranum, bnodenum, enode_num):
    key_file = f"keys/{host.split(':')[0]}.pem"
    global commond_num
    commond_num += 1
    return f"ssh -q -i {key_file} azureuser@{host.split(':')[0]} 'cd /home/azureuser/build && ./BNode --id {index} -u {usernum} -r 1 -p {grpcnum} -c ../config/config_multi_server.json -w {paranum} --bnode-num {bnodenum} --enode-num {enode_num} --use-B' &"

def generate_oram_command(index, host, usernum, grpcnum, paranum, bnodenum, enode_num):
    key_file = f"keys/{host.split(':')[0]}.pem"
    global commond_num
    commond_num += 1
    return f"ssh -q -i {key_file} azureuser@{host.split(':')[0]} 'cd /home/azureuser/build/pack && ./host --port 22001 --user-num {usernum} --bnode-num {bnodenum} --enode-num {enode_num}' &"

def generate_enode_command(index, host, usernum, grpcnum, paranum, bnodenum, enode_num):
    key_file = f"keys/{host.split(':')[0]}.pem"
    global commond_num
    commond_num += 1
    return f"ssh -q -i {key_file} azureuser@{host.split(':')[0]} 'cd /home/azureuser/build && ./ENode --id {index} -u {usernum} -r 1 -p {grpcnum} -c ../config/config_multi_server.json -w {paranum} --bnode-num {bnodenum} --enode-num {enode_num} --use-B' &"

def generate_client_command(index, host, usernum, grpcnum, paranum, bnodenum, enode_num):
    key_file = f"keys/{host.split(':')[0]}.pem"
    global commond_num
    commond_num += 1
    return f"ssh -i {key_file} azureuser@{host.split(':')[0]} 'cd /home/azureuser/build && ./Client --id {index} -u {usernum} -r 1 -p {grpcnum} -c ../config/config_multi_server.json --bnode-num {bnodenum} --enode-num {enode_num}'"

def generate_kill_commands(config):
    kill_commands = []

    def generate_kill_command(host, port):
        key_file = f"keys/{host.split(':')[0]}.pem"
        return f"ssh -i {key_file} azureuser@{host.split(':')[0]} 'sudo kill $(sudo lsof -t -i:{port})' &"

    for host in config.get('bnode_addr', []) + config.get('enode_addr', []) + config.get('clt_addr', []):
        ip, port = host.split(':')
        kill_commands.append(generate_kill_command(host, port))

    return kill_commands

def write_to_bash_script(commands, output_file):
    with open(output_file, 'a') as file:
        file.write("#!/bin/bash\n\n")
        for command in commands:
            file.write(f"{command}\n")

def append_interaction_to_file(file_path):
    bash_script = '''
# 输出提示信息
echo "按 k 清除环境"
while :
do
    read -n 1 -r input
    if [ "$input" = "k" ]; then
        echo "继续运行"
        break
    else
        echo "输入非 'k'，继续等待"
    fi
done
'''

    with open(file_path, 'a') as file:
        file.write(bash_script)

def generate_and_write_kill_commands(output_file):
    global commond_num
    with open(output_file, 'a') as file:
        for i in range(1, commond_num + 1):
            file.write(f"kill $pid{i}\n")

def generate_bash_file_name(usernum, grpcnum, paranum, bnodenum, enode_num):
    usernum_exponent = 0
    while usernum > 1:
        usernum_exponent += 1
        usernum /= 2

    bash_script = f"{bnodenum}e{enode_num}b{usernum_exponent}u{grpcnum}g{paranum}w.sh"
    
    return bash_script

if __name__ == "__main__":
    # bash_script = "run_test.sh"
    usernum = 10000
    grpcnum = 1
    paranum = 8
    bnodenum = 24
    enode_num = 8
    # bash_script = "8e8b10u1g1w.sh"
    bash_script = generate_bash_file_name(usernum, grpcnum, paranum, bnodenum, enode_num)
    
    config_file = "/home/azureuser/muti-boomerang-dev/config/config_multi_server.json"  # Replace with your actual config file path
    # config_file = "/home/azureuser/muti-boomerang-dev/config/config_local.json"
    upload_config(config_file, bash_script)
    upload_build(config_file, bash_script)
    

    

    bash_script_content = ""
    with open(config_file, 'r') as file:
        config = json.load(file)
    
    # commond_num = 0
    bash_script_content += "sleep 5\n"
    # index = 0
    for index, bnode_host in enumerate(config['bnode_addr']):
        bnode_command = generate_bnode_command(index, bnode_host, usernum, grpcnum, paranum, bnodenum, enode_num)
        bash_script_content += f"{bnode_command}\n"

    bash_script_content += "sleep 120\n"
    # index = 0
    for index, enode_host in enumerate(config['enode_addr']):
        enode_command = generate_enode_command(index, enode_host, usernum, grpcnum, paranum, bnodenum, enode_num)
        bash_script_content += f"{enode_command}\n"

    bash_script_content += "sleep 20\n"
    # index = 0
    for index, clt_host in enumerate(config['clt_addr']):
        client_command = generate_client_command(index, clt_host, usernum, grpcnum, paranum, bnodenum, enode_num)
        bash_script_content += f"{client_command}\n"

    with open(bash_script, 'a') as run_test_file:
        run_test_file.write(bash_script_content)

    # generate_and_write_kill_commands(bash_script)

    append_interaction_to_file(bash_script)

    kill_commands = generate_kill_commands(config)
    write_to_bash_script(kill_commands, bash_script)

