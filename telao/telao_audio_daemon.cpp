#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>

const int DELAY_VISUAL_MS = 500;

const std::string PATH = "/home/ipam/atendimento/telao/som/";

struct SenhaItem {
    long long cod_senha;
    std::string nm_senha;
    std::string nm_guiche;
    std::string json_original;
};

std::queue<std::string> fila_senhas;
std::mutex mtx_fila;

// Controle de repeticao: guarda quando cada senha (nm_senha) foi anunciada pela ultima vez.
// Usado apenas na thread principal, por isso nao precisa de mutex.
std::map<std::string, std::chrono::steady_clock::time_point> ultimo_anuncio;
const int COOLDOWN_REPETICAO_MS = 3000; // Mesma senha so pode ser anunciada de novo apos 3s

// Retorna true se a senha nunca foi anunciada ou se ja passaram 3s desde o ultimo anuncio
bool cooldown_liberado(const std::string& nm_senha) {
    auto it = ultimo_anuncio.find(nm_senha);
    if (it == ultimo_anuncio.end()) return true;
    auto decorrido = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - it->second).count();
    return decorrido >= COOLDOWN_REPETICAO_MS;
}

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

    std::string url = "http://127.0.0.1:8080/telao?password=&cod_telao=1";
    //http://127.0.0.1:8080/telao?password=&cod_telao=1
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

void adicionar_numero_a_lista(int num, std::vector<std::string>& audios, const std::string& base_path) {
    char filename[256];

    // Se o número exato existir como arquivo (ex: 1 até 20, ou 100)
    sprintf(filename, "%d.wav", num);
    if (arquivo_existe(base_path + filename)) {
        audios.push_back(filename);
        return;
    }

    if (num >= 100 && num < 1000) {
        int centena = (num / 100) * 100;
        int resto = num % 100;

        // CORREÇÃO: Para o 100 usamos "cem.wav", mas para 101-199 usamos "cento.wav" se existir, ou o conector adequado
        if (num == 100) {
            audios.push_back("100.wav");
            return;
        }

        // Se for na faixa de 101 a 199, o correto na locução é "cento e..."
        if (centena == 100) {
            if (arquivo_existe(base_path + "centoe.wav")) {
                audios.push_back("centoe.wav");
            } else if (arquivo_existe(base_path + "cento.wav")) {
                audios.push_back("cento.wav");
            }
        } else {
            sprintf(filename, "%d.wav", centena);
            if (arquivo_existe(base_path + filename)) {
                audios.push_back(filename);
            }
        }

        if (resto > 0) {
            // Se já tratamos o "centoe.wav" acima, não precisamos duplicar o conector
            if (centena != 100) {
                if (centena == 200 && arquivo_existe(base_path + "duzentose.wav")) {
                    audios.push_back("duzentose.wav");
                } else if (centena == 300 && arquivo_existe(base_path + "trezentose.wav")) {
                    audios.push_back("trezentose.wav");
                } else if (centena == 400 && arquivo_existe(base_path + "quatrocentose.wav")) {
                    audios.push_back("quatrocentose.wav");
                } else if (centena == 500 && arquivo_existe(base_path + "quinhentose.wav")) {
                    audios.push_back("quinhentose.wav");
                }
            }

            sprintf(filename, "%d.wav", resto);
            if (arquivo_existe(base_path + filename)) {
                audios.push_back(filename);
            } else {
                int dezenas = (resto / 10) * 10;
                int unidades = resto % 10;
                if (dezenas > 0) {
                    sprintf(filename, "%d.wav", dezenas);
                    if (arquivo_existe(base_path + filename)) audios.push_back(filename);
                }
                if (unidades > 0) {
                    sprintf(filename, "%d.wav", unidades);
                    if (arquivo_existe(base_path + filename)) audios.push_back(filename);
                }
            }
        }
    } else if (num >= 10 && num < 100) {
        int dezenas = (num / 10) * 10;
        int unidades = num % 10;
        sprintf(filename, "%d.wav", dezenas);
        if (arquivo_existe(base_path + filename)) audios.push_back(filename);
        if (unidades > 0) {
            sprintf(filename, "%d.wav", unidades);
            if (arquivo_existe(base_path + filename)) audios.push_back(filename);
        }
    } else {
        sprintf(filename, "%d.wav", num);
        if (arquivo_existe(base_path + filename)) audios.push_back(filename);
    }
}

std::vector<std::string> mapear_para_audios(const std::string& senha, const std::string& guiche) {
    std::vector<std::string> audios;
    std::string base_path = PATH;
    audios.push_back("notify.wav");
    audios.push_back("senha.wav");

    std::string letra_senha = "";
    std::string num_str_senha = "";

    for (char c : senha) {
        if (c >= 'A' && c <= 'Z') {
            audios.push_back(std::string(1, tolower(c)) + ".wav");
        } else if (c >= 'a' && c <= 'z') {
            audios.push_back(std::string(1, c) + ".wav");
        } else if (c >= '0' && c <= '9') {
            num_str_senha += c;
        }
    }

    if (!num_str_senha.empty()) {
        int num_s = std::stoi(num_str_senha);
        adicionar_numero_a_lista(num_s, audios, base_path);
    }

    audios.push_back("guiche.wav");
    if (!guiche.empty()) {
        int num_g = std::stoi(guiche);
        adicionar_numero_a_lista(num_g, audios, base_path);
    }

    return audios;
}

void reproduzir_senha(const std::string& json_item) {
    std::string nm_senha = extrair_valor_json(json_item, "nm_senha");
    std::string nm_guiche = extrair_valor_json(json_item, "nm_guiche");

    if (nm_senha.empty() || nm_guiche.empty()) return;

    if (DELAY_VISUAL_MS > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_VISUAL_MS));
    }

    std::vector<std::string> playlist = mapear_para_audios(nm_senha, nm_guiche);
    std::string base_path = PATH;

    std::cout << "[ANUNCIANDO] Senha: " << nm_senha << " | Guiche: " << nm_guiche << "\n";

    for (const auto& arquivo : playlist) {
        std::string caminho_completo = base_path + arquivo;
        if (arquivo_existe(caminho_completo)) {
            std::string cmd = "env -u LD_PRELOAD paplay \"" + caminho_completo + "\" >/dev/null 2>&1";
            int resultado = system(cmd.c_str());
            (void)resultado; // Ignora explicitamente o aviso de retorno não utilizado
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
            usleep(1500000); // 1.5s de pausa entre as senhas
        } else {
            usleep(100000);
        }
    }
}

// (Mantenha todas as funções anteriores: WriteCallback, extrair_valor_json, consultar_api_json, arquivo_existe, adicionar_numero_a_lista, mapear_para_audios, reproduzir_senha, worker_fila)

int main() {
    std::thread t(worker_fila);
    t.detach();

    long long maior_cod_visto = -1;
    std::string nm_ultima_senha = "";

    // >>> PASSO DE SINCRONIZACAO INICIAL <<<
    // Descobre o maior ID atual da API para ignorar o historico antigo ao iniciar
    std::string json_inicial = consultar_api_json();
    if (!json_inicial.empty()) {
        size_t pos_inicio = 0;
        while ((pos_inicio = json_inicial.find('{', pos_inicio)) != std::string::npos) {
            size_t pos_fim = json_inicial.find('}', pos_inicio);
            if (pos_fim == std::string::npos) break;

            std::string objeto_json = json_inicial.substr(pos_inicio, pos_fim - pos_inicio + 1);
            std::string cod_str = extrair_valor_json(objeto_json, "cod_senha");

            if (!cod_str.empty()) {
                long long cod = std::stoll(cod_str);
                if (cod > maior_cod_visto) {
                    maior_cod_visto = cod;
                    nm_ultima_senha = extrair_valor_json(objeto_json, "nm_senha");
                }
            }
            pos_inicio = pos_fim + 1;
        }
    }
    // Marca a ultima senha como "recem anunciada" para que as linhas antigas
    // reexibidas pelo tail -F na inicializacao nao disparem uma repeticao indevida
    if (!nm_ultima_senha.empty()) {
        ultimo_anuncio[nm_ultima_senha] = std::chrono::steady_clock::now();
    }
    std::cout << "[DAEMON] Sincronizado. Ignorando IDs antigos ate: " << maior_cod_visto << "\n";
    // ----------------------------------------

    std::string comando_tail = "tail -F telao_logs.txt 2>/dev/null";
    FILE* pipe = popen(comando_tail.c_str(), "r");
    if (!pipe) {
        std::cerr << "[ERRO] Não foi possível iniciar o pipe de monitoramento de logs.\n";
        return 1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        std::string linha(buffer);

        if (linha.find("fala") != std::string::npos || 
            linha.find("Play") != std::string::npos || 
            linha.find("notify") != std::string::npos || 
            linha.find("Som path") != std::string::npos) {
            
            usleep(150000);

            std::string json = consultar_api_json();
            if (json.empty()) continue;

            std::vector<SenhaItem> novas_senhas;
            SenhaItem ultimo_api;            // Senha de maior ID retornada pela API (candidata a repeticao)
            bool tem_ultimo_api = false;
            size_t pos_inicio = 0;

            while ((pos_inicio = json.find('{', pos_inicio)) != std::string::npos) {
                size_t pos_fim = json.find('}', pos_inicio);
                if (pos_fim == std::string::npos) break;

                std::string objeto_json = json.substr(pos_inicio, pos_fim - pos_inicio + 1);
                std::string cod_str = extrair_valor_json(objeto_json, "cod_senha");

                if (!cod_str.empty()) {
                    long long cod = std::stoll(cod_str);
                    if (cod > maior_cod_visto) {
                        SenhaItem item;
                        item.cod_senha = cod;
                        item.nm_senha = extrair_valor_json(objeto_json, "nm_senha");
                        item.nm_guiche = extrair_valor_json(objeto_json, "nm_guiche");
                        item.json_original = objeto_json;
                        novas_senhas.push_back(item);
                    }

                    // Guarda a senha mais recente da API, para o caso de repeticao
                    if (!tem_ultimo_api || cod > ultimo_api.cod_senha) {
                        ultimo_api.cod_senha = cod;
                        ultimo_api.nm_senha = extrair_valor_json(objeto_json, "nm_senha");
                        ultimo_api.nm_guiche = extrair_valor_json(objeto_json, "nm_guiche");
                        ultimo_api.json_original = objeto_json;
                        tem_ultimo_api = true;
                    }
                }
                pos_inicio = pos_fim + 1;
            }

            if (!novas_senhas.empty()) {
                std::sort(novas_senhas.begin(), novas_senhas.end(), [](const SenhaItem& a, const SenhaItem& b) {
                    return a.cod_senha < b.cod_senha;
                });

                std::lock_guard<std::mutex> lock(mtx_fila);
                for (const auto& item : novas_senhas) {
                    if (item.cod_senha > maior_cod_visto) {
                        fila_senhas.push(item.json_original);
                        maior_cod_visto = item.cod_senha;
                        ultimo_anuncio[item.nm_senha] = std::chrono::steady_clock::now(); // Registra o anuncio
                        std::cout << "[ENFILEIRADO] Senha ID " << item.cod_senha << " (" << item.nm_senha << " - Guiche " << item.nm_guiche << ")\n";
                    }
                }
            } else if (tem_ultimo_api && ultimo_api.cod_senha == maior_cod_visto &&
                       cooldown_liberado(ultimo_api.nm_senha)) {
                // Nenhuma senha nova: o operador pediu para repetir a ultima senha chamada.
                // So repete se ja passaram 3s do ultimo anuncio (evita duplicar as varias linhas de log de uma mesma chamada).
                std::lock_guard<std::mutex> lock(mtx_fila);
                fila_senhas.push(ultimo_api.json_original);
                ultimo_anuncio[ultimo_api.nm_senha] = std::chrono::steady_clock::now();
                std::cout << "[REPETIDO] Senha ID " << ultimo_api.cod_senha << " (" << ultimo_api.nm_senha << " - Guiche " << ultimo_api.nm_guiche << ")\n";
            }
        }
    }

    pclose(pipe);
    return 0;
}
