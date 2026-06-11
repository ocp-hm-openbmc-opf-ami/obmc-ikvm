#!/bin/sh

service_name="start-ipkvm"
HOST="localhost"

# If KVM service is disabled, skip trigger
if ! systemctl is-enabled --quiet ${service_name}.service; then
    echo "Service disabled, skipping trigger"
    exit 0
fi

# Get the listen property
listen_output=$(systemctl show ${service_name}.socket --property=Listen)

# Extract the port number
PORT=$(echo "$listen_output" | grep -oE ':[0-9]+' | grep -oE '[0-9]+' | tail -n 1)
echo "KVM Server listening on Port: $PORT"

# Wait for VNC port to be ready (max 3 attempts)
echo "Connecting to $HOST:$PORT"
attempts=0
while ! exec 3<>/dev/tcp/$HOST/$PORT 2>/dev/null; do
    attempts=$((attempts + 1))
    if [ $attempts -ge 3 ]; then
        echo "Failed to connect to $HOST:$PORT"
        exit 1
    fi
    sleep 1
done

echo "Connected to $HOST:$PORT"
echo -e "Dummy-Client" >&3
sleep 2
exec 3<&-
exec 3>&-
echo "Disconnected from $HOST:$PORT"
