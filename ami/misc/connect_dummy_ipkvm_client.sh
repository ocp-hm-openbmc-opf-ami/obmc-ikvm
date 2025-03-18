#!/bin/sh

socket_file_name="start-ipkvm"
HOST="localhost"

# Get the listen property
listen_output=$(systemctl show $socket_file_name.socket --property=Listen)

# Extract the port number
PORT=$(echo "$listen_output" | grep -oE ':[0-9]+' | grep -oE '[0-9]+' | tail -n 1)
# Print the port number
echo "$PORT"

# connect and disconnect dummy client
echo "Connecting to $HOST:$PORT"
exec 3<>/dev/tcp/$HOST/$PORT
if [ $? -eq 0 ]; then
    echo "Connected to $HOST:$PORT"
    echo -e "Dummy-Client" >&3
    sleep 2  # Wait for 2 seconds
    exec 3<&-
    exec 3>&-
    echo "Disconnected from $HOST:$PORT"
else
    echo "Failed to connect to $HOST:$PORT"
fi
