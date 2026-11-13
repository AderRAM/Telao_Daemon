#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>

const int DELAY_VISUAL_MS = 500;

std::queue<std::string> fila_senhas;
std::mutex mtx_fila;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string extrair_valor_json(const std::string& json, const std::string& chave) {
    std::string busca = "\"" + chave + "\":";
    size_t pos = json.find(busca);
    if (pos == std::string::npos) return "";

    pos += busca.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) {
        if (json[pos] == '"') { pos++; break; }
        pos++;
    }

    size_t fim = json.find_first_of(",}\"", pos);
    if (fim == std::string::npos) return "";

    return json.substr(pos, fim - pos);
}

std::string consultar_api_json() {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    // Mude para o servidor de produção se necessário: ""
    std::string url = "http://127.0.0.1:8080/telao?password=&cod_telao=1";
    std::string readBuffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";
    return readBuffer;
}

bool arquivo_existe(const std::string& caminho) {
    struct stat buffer;
    return (stat(caminho.c_str(), &buffer) == 0);
}

std::vector<std::string> mapear_para_audios(const std::string& senha, const std::string& guiche) {
    std::vector<std::string> audios;
    audios.push_back("senha.wav");

    for (char c : senha) {
        if (c >= 'A' && c <= 'Z') {
            std::string s(1, tolower(c));
            audios.push_back(s + ".wav");
        } else if (c >= 'a' && c <= 'z') {
            std::string s(1, c);
            audios.push_back(s + ".wav");
        } else if (c >= '0' && c <= '9') {
            std::string s(1, c);
            audios.push_back(s + ".wav");
        }
    }

    audios.push_back("guiche.wav");
    for (char c : guiche) {
        if (c >= '0' && c <= '9') {
            std::string s(1, c);
            audios.push_back(s + ".wav");
        }
    }

    return audios;
}

void reproduzir_senha(const std::string& json) {
    std::string nm_senha = extrair_valor_json(json, "nm_senha");
    std::string nm_guiche = extrair_valor_json(json, "nm_guiche");

    if (nm_senha.empty() || nm_guiche.empty()) return;

    if (DELAY_VISUAL_MS > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_VISUAL_MS));
    }

    std::vector<std::string> playlist = mapear_para_audios(nm_senha, nm_guiche);
    std::string base_path = "/home/ipam/Documentos/telao/som/";

    std::cout << "[ANUNCIANDO] Senha: " << nm_senha << " | Guiche: " << nm_guiche << "\n";

    for (const auto& arquivo : playlist) {
        std::string caminho_completo = base_path + arquivo;
        if (arquivo_existe(caminho_completo)) {
            std::string cmd = "paplay \"" + caminho_completo + "\" >/dev/null 2>&1";
            system(cmd.c_str());
        }
    }
}

void worker_fila() {
    while (true) {
        std::string json_atual = "";
        {
            std::lock_guard<std::mutex> lock(mtx_fila);
            if (!fila_senhas.empty()) {
                json_atual = fila_senhas.front();
                fila_senhas.pop();
            }
        }

        if (!json_atual.empty()) {
            reproduzir_senha(json_atual);
            usleep(500000);
        } else {
            usleep(100000);
        }
    }
}

int main() {
    std::thread t(worker_fila);
    t.detach();

    // Garante que o arquivo de log existe para evitar erros de abertura
    system("touch telao_logs.txt");

    std::cout << "[DAEMON] Iniciado com leitura segura de logs estilo tail...\n";

    std::string ultimo_cod_senha = "";
    FILE* file = fopen("telao_logs.txt", "r");
    if (file) {
        fseek(file, 0, SEEK_END); // Começa a ler apenas o que entrar a partir de agora
    }

    char buffer[256];

    while (true) {
        if (!file) {
            file = fopen("telao_logs.txt", "r");
            if (!file) {
                usleep(500000);
                continue;
            }
            fseek(file, 0, SEEK_END);
        }

        if (fgets(buffer, sizeof(buffer), file) != NULL) {
            // Imprime opcionalmente para debug no terminal do daemon se necessário
            // std::cout << "[LOG LIDO]: " << buffer;

            if (strstr(buffer, "notify") != NULL || strstr(buffer, "fala") != NULL || strstr(buffer, "Play") != NULL) {
                usleep(200000); // Aguarda a API atualizar

                std::string json = consultar_api_json();
                std::string cod_senha = extrair_valor_json(json, "cod_senha");

                if (!cod_senha.empty() && cod_senha != ultimo_cod_senha) {
                    std::lock_guard<std::mutex> lock(mtx_fila);
                    fila_senhas.push(json);
                    ultimo_cod_senha = cod_senha;
                    std::cout << "[ENFILEIRADO] Nova senha adicionada à fila: " << cod_senha << "\n";
                }
            }
        } else {
            // Se chegou ao fim do arquivo, aguarda um pouco e verifica se o arquivo foi truncado/recriado
            usleep(100000);
            clearerr(file);
        }
    }

    if (file) fclose(file);
    return 0;
}
