# Visão Geral da lazylang

Este documento descreve as decisões fundamentais de design da linguagem lazylang.

---

## Princípios

* Explícito é melhor que implícito
* Leitura é mais importante que escrita
* Poucas abstrações, bem definidas
* Erros devem ser detectados o mais cedo possível

---

## Sintaxe

* Blocos definidos por indentação
* Sem chaves ou ponto e vírgula
* Funções são expressões

---

## Tipos

* Tipos primitivos: int, float, bool, string, null
* Structs imutáveis
* maybe[T] para ausência de valor
* result[T, E] para falhas

---

## Concorrência

* Baseada em `task` e `future`
* Comunicação via `chan[T]`
* Nenhum estado mutável compartilhado

---

## Status

Este documento evoluirá conforme a linguagem amadurece.
