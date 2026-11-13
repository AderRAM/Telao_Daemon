from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import urllib.parse
import threading
import time

# Lista de senhas simuladas
SENHAS_MOCK = [
    {"cod_senha": 288210, "nm_senha": "A-040", "nm_guiche": "01"},
    {"cod_senha": 288211, "nm_senha": "A-241", "nm_guiche": "04"},
    {"cod_senha": 288212, "nm_senha": "A-342", "nm_guiche": "02"},
    {"cod_senha": 288213, "nm_senha": "A-043", "nm_guiche": "03"},
    {"cod_senha": 288214, "nm_senha": "B-101", "nm_guiche": "01"},
    {"cod_senha": 288215, "nm_senha": "B-082", "nm_guiche": "02"},
]

indice_atual = 0  # Começa da primeira e vai avançando

class MockAPIHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global indice_atual
        parsed_path = urllib.parse.urlparse(self.path)
        
        if parsed_path.path == "/telao":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            
            # Retorna em formato de array decrescente (igual ao Yii: mais recente primeiro)
            dados_para_retornar = SENHAS_MOCK[indice_atual]
            
            self.wfile.write(json.dumps(dados_para_retornar).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Silencia o log HTTP padrão (GET /telao...) para não poluir o terminal
        return

def avancar_senhas_automaticamente():
    global indice_atual
    while True:
        time.sleep(5)  # Avança para a próxima senha a cada 5 segundos (ajuste se quiser)
        indice_atual = (indice_atual + 1) % len(SENHAS_MOCK)
        s = SENHAS_MOCK[indice_atual]
        print(f"\n[API MOCK] >>> Nova senha ativa na fila: {s['nm_senha']} (Guiche {s['nm_guiche']}) (ID: {s['cod_senha']})")

def run(server_class=HTTPServer, handler_class=MockAPIHandler, port=8080):
    server_address = ('127.0.0.1', port)
    httpd = server_class(server_address, handler_class)
    
    # Inicia thread para trocar de senha periodicamente
    t = threading.Thread(target=avancar_senhas_automaticamente, daemon=True)
    t.start()
    
    print(f"=== API MOCK LOCAL RODANDO EM http://127.0.0.1:{port}/telao ===")
    print("As senhas vão alternar automaticamente a cada 15 segundos para seus testes.")
    print("Pressione Ctrl+C para encerrar.\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    run()
