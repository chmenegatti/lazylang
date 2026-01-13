# lazylang

**lazylang** é uma linguagem de programação **compilada**, **minimalista** e **opinativa**, projetada para **backends modernos**, CLIs e serviços concorrentes.

Extensão oficial: **.lz**
Compilador: **lazylangc**

---

## Visão Geral

A lazylang prioriza:

* Clareza sobre concisão extrema
* Poucas palavras reservadas
* Comportamento previsível
* Erros explícitos e verificáveis em tempo de compilação

Ela elimina complexidade acidental e evita mecanismos implícitos difíceis de depurar.

---

## Principais Características

* Compilada para **binário nativo**
* **Indentação significativa** (estilo Python)
* Tipagem **explícita e forte**
* **Imutabilidade por padrão**
* Gerenciamento de memória via **ARC simplificado**
* Concorrência segura baseada em tarefas
* Tratamento de erros **sem exceções**
* Sistema de módulos simples e determinístico

---

## Exemplo Rápido

```lz
main: () -> null = ()
    log("Hello, lazylang")
```

---

## Escopo

### Dentro do escopo

* Backends HTTP
* CLIs
* Workers e serviços concorrentes

### Fora do escopo

* Sistemas embarcados
* Frontend
* Game engines
* Data science

---

## Estado do Projeto

🚧 Em desenvolvimento ativo

* Lexer completo (com INDENT/DEDENT)
* Parser, AST, Sema e Codegen em desenvolvimento

---

## Estrutura do Repositório

```text
lazylang/
├── src/
├── tests/
├── docs/
├── Makefile
├── README.md
├── CONTRIBUTING.md
└── ROADMAP.md
```

---

## Documentação

Consulte a pasta [`docs/`](./docs) para documentação detalhada sobre:

* Design da linguagem
* Gramática
* Arquitetura do compilador

---

## Licença

A definir (MIT recomendada).
