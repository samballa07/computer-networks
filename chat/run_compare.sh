#! /bin/bash

# Preset variables referenced later
submission_dir="$(pwd)"
con_name="rserver"
cl_con_name="to_hosted"
dump_pcap="dump"

# Default values
hosted_port=4501
delete=0
name=0

# We want to shut down the containers and backgroud processes upon finishing
function clean_up {

    # Kill the tcpdump, if it was running
    if  [[ 1 == ${record} ]]
    then
        sudo pkill -f tcpdump
    fi

    # Delete the temporary input files
    rm ${hosted_input} ${your_input} ${your_second_input} ${hosted_second_input}

    # Shut down containers
    echo "Shutting down YOUR server..."
    docker stop ${con_name}

    docker ps | grep -q ${cl_con_name}
    hosted_up=$?

    if [[ 0 == ${hosted_up} ]]
    then
        echo "Shutting down the client container..."
        docker stop ${cl_con_name}
    fi
}

trap clean_up EXIT

# Input file should be the first argument
input=$1
shift

# Check it exists and is executable
if [[ ! -f $input || ! -x $input ]]
then
    echo "ERROR: An executable input file must be supplied. Check the path is right and permissions are set."
    exit 1
fi

# !!!!!!!!!!!!!!!!!!! CHECK !!!!!!!!!!!!!!!!!
# Rename this if you want a different folder
output_dir="./tests"
mkdir -p ${output_dir}

while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
    -p|--port)
    hosted_port="$2"
    shift
    shift
    ;;
    -d|--delete)
    delete=1
    shift
    ;;
    -r|--record)
    record=1
    shift
    ;;
    -n|--name)
    name="$2"
    shift
    shift
    ;;
    -s|--second)
    second_input=$2
    shift
    shift
    ;;
    *)
    echo "Unknown command line argument"
    exit 1
    shift
    ;;
esac
done

# Run make in the container
echo "Running make..."
docker run --rm -v "${submission_dir}/assignment2:/opt" -w /opt baseline make

# Start up the user's server on a randomized port
server_port=$((20000 + ($RANDOM % 10000)))
echo "Starting test server on port $server_port..."
docker run --rm -d --net=host --name ${con_name} -v "${submission_dir}:/opt" baseline /opt/assignment2/rserver -p ${server_port}

# Give the server time to start up
sleep 1

# Configure the names of the output files, based on the user's name provided
if [[ 0 == ${name} ]]; then
    yourOutputFn="yourOutput.XXX"
    hostedOutputFn="hostedOutput.XXX"
else
    yourOutputFn="${name}_yourOutput.XXX"
    hostedOutputFn="${name}_hostedOutput.XXX"
    dump_pcap="${name}_dump"
fi

# ------------- Running the client(s) on YOUR server -----------------

# Create the temp files for input and output for the user's server
your_input=$(mktemp --tmpdir=${output_dir} -t yourInput.XXX)
your_output=$(mktemp --tmpdir=${output_dir} -t ${yourOutputFn})

# Will use tcpdump in the container to record packets if desired
if [[ 1 == ${record} ]]
then
    echo "Recording your data..."
    docker exec -d -t ${con_name} sudo tcpdump -U -i lo -w "/opt/${output_dir}/${dump_pcap}_yours.pcap"
fi

# Compile the client's commands. Automatically connects to server for you
echo "\connect 127.0.0.1:${server_port}" >> ${your_input}
${input} >> ${your_input}

if [[ -x ${second_input} ]]
then
    # Create the secondary in/output files
    yourSecondOutputFn="Second_${yourOutputFn}"
    your_second_output=$(mktemp --tmpdir=${output_dir} -t ${yourSecondOutputFn})
    your_second_input=$(mktemp --tmpdir=${output_dir} -t yourSecondInput.XXX)

    # Prepend the connect and compile the commands
    echo "\connect 127.0.0.1:${server_port}" >> ${your_second_input}
    ${second_input} >> ${your_second_input}

    # Run the two clients
    echo "Running reference clients against YOUR server... output will be avaiable in ${your_output} and ${your_second_output}"
    ./run_two.sh ${con_name} ${your_input} ${your_output} ${your_second_input} ${your_second_output}
else
    # Run the single client
    echo "Running reference client against YOUR server... output will be avaiable in ${your_output}"
    docker exec ${con_name} bash -c "/opt/echo_file.sh opt/${your_input} | /opt/provided/client" > ${your_output}
fi


# ------------- Running the client(s) on the HOSTED server -----------------

# Create the temp files for input and output for the hosted server
hosted_input=$(mktemp --tmpdir=${output_dir} -t hostedInput.XXX)
hosted_output=$(mktemp --tmpdir=${output_dir} -t ${hostedOutputFn})


# Will use tcpdump in the container to record packets if desired
if [[ 1 == ${record} ]]
then
    echo "Recording hosted data..."
    sudo tcpdump -U -i any -w "${output_dir}/${dump_pcap}_hosted.pcap" &
fi


# Compile the client's commands. Automatically connects to server for you
echo "\connect 128.8.130.3:${hosted_port}" >>  ${hosted_input}
${input} >> ${hosted_input}

if [[ -x ${second_input} ]]
then
    # Create the secondary in/output files
    hostedSecondOutputFn="Second_${hostedOutputFn}"
    hosted_second_output=$(mktemp --tmpdir=${output_dir} -t ${hostedSecondOutputFn})
    hosted_second_input=$(mktemp --tmpdir=${output_dir} -t hostedSecondInput.XXX)

    # Compile the connect and second input file commands
    echo "\connect 128.8.130.3:${hosted_port}" >>  ${hosted_second_input}
    ${second_input} >> ${hosted_second_input}

    # Run the two clients
    echo "Running reference clients against HOSTED server... output will be available in ${hosted_output} ${hosted_second_output}"
    docker run --rm -d --net=host --name to_hosted -v "${submission_dir}:/opt" baseline tail -f /dev/null
    ./run_two.sh ${cl_con_name} ${hosted_input} ${hosted_output} ${hosted_second_input} ${hosted_second_output}
else
    # Run the single client
    echo "Running reference client against HOSTED server... output will be available in ${hosted_output}"
    docker run --rm --net=host --name ${cl_con_name} -v "${submission_dir}:/opt" baseline bash -c "/opt/echo_file.sh opt/${hosted_input} | /opt/provided/client" > $hosted_output
fi

# ------------- Checking the ouputs -----------------
if [[ -f ${second_input} ]]
then
    echo "Diffing your FIRST clients outputs..."
    diff ${hosted_output} ${your_output}
    rc=$?

    if [ -0 -ne $rc ]
    then
        echo "ERROR: Your FIRST client outputs did NOT match." >& 2
        echo "Try comparing ${hosted_output} and ${your_output} to ensure this was not due to sharing the hosted server with another user" >&2
    fi

    echo
    echo "Diffing your SECOND clients outputs..."
    diff ${hosted_second_output} ${your_second_output}
    rc_two=$?

    if [ -0 -ne $rc_two ]
    then
        echo "ERROR: Your SECOND client outputs did NOT match." >& 2
        echo "Try comparing ${hosted_second_output} and ${your_second_output} to ensure this was not due to sharing the hosted server with another user" >&2
    fi
else
    echo "Diffing your FIRST clients outputs..."
    diff ${hosted_output} ${your_output}
    rc=$?

    # Check if there was a difference
    if [ -0 -ne $rc ]
    then
        echo "ERROR: The two outputs did NOT match." >& 2
        echo "Try comparing ${hosted_output} and ${your_output} to ensure this was not due to sharing the hosted server with another user" >&2
        exit 1
    else
        echo "SUCCESS: Correct output"
    fi
fi

# Delete the output files if you don't want them
if [[ 1 == ${delete} ]]
then
    rm ${hosted_output} ${your_output} ${hosted_second_output} ${your_second_output}
fi

# Wireshark suggestions
if [[ 1 == ${record} ]]
then
    echo "Try using the filter tcp.port == ${server_port} in Wireshark for your server tcp data."
    echo "Try using the:w
     filter ip.addr == 128.8.130.3 in Wireshark for the hosted tcp data."
fi
