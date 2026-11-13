#!/bin/bash

# Diretório onde estão os logs e arquivos
DIR="/home/ipam/Documentos/telao"
cd "$DIR" || exit 1

URL="http://127.0.0.1:8080/telao?password=&cod_telao=1"

echo "====================================================="
echo "   OBSERVADOR EM TEMPO REAL (API vs LOGS DO TELÃO)   "
echo "====================================================="

# Função para extrair valor simples de JSON sem dependências pesadas (como jq)
extrair_json() {
    local json="$1"
    local chave="$2"
    # Pega apenas a primeira ocorrência da chave no JSON e limpa os caracteres de formatação
    echo "$json" | grep -o "\"$chave\":[^\,}]*" | head -n 1 | sed 's/.*://; s/"//g; s/ //g'
}

# Processo 1: Monitorar a API a cada 1 segundo
monitorar_api() {
    local ultima_senha=""
    while true; do
        # Faz a requisição cURL com timeout curto
        resposta_json=$(curl -s --max-time 2 "$URL")
        
        if [ -n "$resposta_json" ]; then
            cod_senha=$(extrair_json "$resposta_json" "cod_senha")
            nm_senha=$(extrair_json "$resposta_json" "nm_senha")
            nm_guiche=$(extrair_json "$resposta_json" "nm_guiche")

            if [ "$cod_senha" != "$ultima_senha" ] && [ -n "$cod_senha" ]; then
                timestamp=$(date '+%H:%M:%S')
                echo "[$timestamp] [API] Nova senha detectada! ID: $cod_senha | Senha: $nm_senha | Guichê: $nm_guiche"
                ultima_senha="$cod_senha"
            fi
        fi
        sleep 1
    done
}

# Processo 2: Monitorar os logs do telão em tempo real com tail
monitorar_logs() {
    echo "[LOGS] Iniciando monitoramento resiliente com tail -F..."
    
    # O -F reconecta automaticamente se o binário apagar e recriar o arquivo
    tail -F telao_logs.txt | while read -r linha; do
        if echo "$linha" | grep -qE "notify|fala|Play"; then
            timestamp=$(date '+%H:%M:%S')
            echo "[$timestamp] [TELAO LOG REAL] $linha"
        fi
    done
}

# Executa as duas funções em background simultaneamente
monitorar_api &
PID_API=$!

monitorar_logs &
PID_LOGS=$!

# Tratamento para encerrar limpo com Ctrl+C
trap "kill $PID_API $PID_LOGS 2>/dev/null; exit 0" SIGINT SIGTERM

# Mantém o script rodando
wait
