#!/bin/bash

echo
echo "---> $0 <---"
echo
let "NumberOfClients = $1"

osascript -e 'tell app "Terminal"
    do script  "cd
                cd Projects/IPC_chat/Server/cmake-build-debug
                ./Server"
end tell'

for ((i = 0; i < NumberOfClients; ++i))
do
osascript -e 'tell app "Terminal"
    do script  "cd
                cd Projects/IPC_chat/Client/cmake-build-debug
                ./Client"
end tell'
done

exit 0
