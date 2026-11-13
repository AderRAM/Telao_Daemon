#!/bin/bash

# Path principal hardcodado
cd /home/ipam/atendimento/telao/

export PATH="/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export LD_LIBRARY_PATH=".:/usr/lib/x86_64-linux-gnu"
export SDL_AUDIODRIVER=alsa
export AUDIODEV=default
export DISPLAY=:0

cleanup() {
    echo -e "\n[ENCERRANDO] Parando daemon, rotina de limpeza e processos do telão..."
    pkill -9 -f telao_audio_daemon 2>/dev/null
    pkill -9 -f ./telao 2>/dev/null
    kill $PID_LIMPEZA 2>/dev/null
    kill $PID_CLIQUE 2>/dev/null
    echo "Sistema encerrado com segurança."
    exit 0
}

trap cleanup SIGINT SIGTERM

# Mata instâncias anteriores
pkill -9 -f telao_audio_daemon 2>/dev/null
pkill -9 -f ./telao 2>/dev/null

# Garante que o arquivo exista e esteja limpo
> telao_logs.txt

echo "=== SISTEMA DO TELÃO EM EXECUÇÃO ==="
echo "Iniciando daemon de áudio em background..."
./telao_audio_daemon &

# Rotina autônoma de gerenciamento de logs em background (a cada 2 horas)
(
    while true; do
        sleep 7200 # 2 horas em segundos
        if [ -f "telao_logs.txt" ]; then
            tamanho=$(stat -c%s "telao_logs.txt" 2>/dev/null || echo 0)
            if [ "$tamanho" -gt 524288 ]; then
                tail -c 262144 "telao_logs.txt" > "telao_logs.tmp" && mv "telao_logs.tmp" "telao_logs.txt"
            fi
        fi
    done
) &
PID_LIMPEZA=$!

# Oculta o cursor após a inicialização
(
    sleep 8
    if command -v xdotool &> /dev/null; then
        xdotool key alt+Tab
    fi
) &
PID_FOCO=$!

echo "Iniciando binário legado do telão..."
echo "Pressione [Ctrl+C] para encerrar a qualquer momento."
echo "-----------------------------------------------------"

# stdbuf -oL força o flush imediato de cada linha para o arquivo telao_logs.txt e para o terminal
stdbuf -oL ./telao 2>&1 | stdbuf -oL tee telao_logs.txt

cleanup
