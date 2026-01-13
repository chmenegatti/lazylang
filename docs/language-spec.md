# lazylang — Especificação da Linguagem (Living Document)

> **Status:** pré‑alpha
> **Objetivo:** servir como documento canônico de referência durante a implementação do compilador e futuras contribuições.

Este documento **não é um tutorial**. Ele define **regras, decisões e invariantes** da linguagem lazylang.

---

## 1. Identidade

* **Nome:** lazylang
* **Extensão:** `.lz`
* **Categoria:** linguagem compilada, estática
* **Domínio:** backends, CLIs, serviços
* **Filosofia:** simplicidade rigorosa

---

## 2. Princípios Fundamentais

1. Explícito > implícito
2. Leitura > escrita
3. Poucas palavras reservadas
4. Erros detectados o mais cedo possível
5. Imutabilidade por padrão
6. Nenhuma "magia" invisível ao usuário

Esses princípios **não são negociáveis**.

---

## 3. Escopo

### 3.1 Dentro do escopo

* Backends HTTP
* CLIs
* Workers
* Serviços concorrentes

### 3.2 Fora do escopo

* Sistemas embarcados
* Frontend
* Game engines
* Data science
* Linguagem de script dinâmica

---

## 4. Sintaxe Geral

* Blocos definidos por **indentação significativa**
* Sem `{}`, `;` ou palavras de fechamento
* Espaços e tabs contam igualmente (1 unidade)
* Comentários iniciam com `#`

Exemplo:

```lz
if condition
    doSomething()
```

---

## 5. Estrutura de Programa

* Um projeto é compilado como **unidade única**
* Arquivo de entrada obrigatório: `main.lz`

---

## 6. Sistema de Módulos

* Um diretório = um módulo
* Um arquivo = um submódulo
* Imports sempre explícitos e no topo do arquivo
* Tudo é **privado por padrão**

```lz
import domain.user
```

### 6.1 Visibilidade

* `pub` exporta símbolos
* Sem `pub`, símbolo é visível apenas no arquivo

---

## 7. Tipos

### 7.1 Tipos Primitivos

* `int` (positivo e negativo)
* `float` (positivo e negativo)
* `bool`
* `string`
* `null`

### 7.2 Tipos Compostos

* `struct` (imutável)
* `maybe[T]` — ausência de valor esperada
* `result[T, E]` — erro explícito
* `future[T]`
* `chan[T]`

---

## 8. Variáveis e Imutabilidade

* Variáveis são **imutáveis por padrão**
* Mutabilidade exige `mut`

```lz
x: int = 10
mut y: int = 20
```

---

## 9. Structs

* Sempre imutáveis
* Sem herança
* Apenas composição

```lz
struct User
    id: int
    name: string
```

Atualização gera cópia:

```lz
u2 = u1 with name = "Ana"
```

---

## 10. Funções

* Funções são **expressões anônimas**
* Retorno implícito da última expressão

```lz
sum: (int, int) -> int = (a, b)
    a + b
```

---

## 11. Controle de Fluxo

### 11.1 Condicionais

* Apenas `if` / `else`

```lz
if x > 0
    positive()
else
    negative()
```

### 11.2 Iteração

* Apenas `for in`

```lz
for item in items
    process(item)
```

---

## 12. Tratamento de Erros

### 12.1 Princípios

* Não existem exceções
* Erros são valores
* Ignorar erro é erro de compilação

### 12.2 result[T, E]

```lz
readFile: (string) -> result[string, FileError]
```

Uso obrigatório:

```lz
res = readFile("a.txt")
if res is err
    log(res.error)
else
    log(res.value)
```

---

## 13. maybe[T]

* Representa ausência válida de valor
* Nunca misturar com `result`

```lz
findUser: (int) -> maybe[User]
```

---

## 14. Concorrência

### 14.1 Modelo

* Baseado em `task`, `future`, `chan`
* Concorrência sempre explícita

```lz
t = task ()
    work()

r = await(t)
```

### 14.2 Regras de Segurança

* Nenhum estado mutável compartilhado
* Structs imutáveis podem ser compartilhadas
* Variáveis `mut` não podem ser capturadas por `task`

---

## 15. Gerenciamento de Memória

* ARC simplificado
* Determinístico
* Sem GC
* Sem `free`

---

## 16. Palavras Reservadas (Atual)

```text
if
else
for
in
struct
mut
maybe
with
pub
import
is
task
return
```

---

## 17. Compilador

* Implementado em **C (ISO C11)**
* Binário: `lazylangc`

### Pipeline

```
.lz
 → Lexer (indentação significativa)
 → Parser (recursive descent)
 → AST
 → Análise Semântica
 → Geração de Código
 → Binário nativo
```

---

## 18. Estado Atual da Implementação

* [x] Lexer completo
* [x] INDENT / DEDENT
* [ ] Parser
* [ ] AST
* [ ] Sema
* [ ] Codegen

---

## 19. Invariantes Importantes

Estas regras **nunca devem ser quebradas**:

* Nada é público por padrão
* Nada é mutável por padrão
* Erros nunca são implícitos
* Concorrência nunca é automática

---

## 20. Uso deste Documento

* Deve ficar em `docs/language-spec.md`
* Deve ser consultado **antes de qualquer implementação nova**
* Mudanças devem ser discutidas via Issues

Este é o **contrato técnico** da lazylang.
