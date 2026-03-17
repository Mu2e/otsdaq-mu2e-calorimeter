#!/bin/bash

# Cartella contenente i file
DIR="./"  # puoi cambiarlo in un path assoluto

# Nome del file di output
SUMMARY_FILE="summary.log"
rightnow=$(date +'%Y%m%d_%H%M')
SUMMARY_DATE="summary_${rightnow}.log"

# Cancella o crea file riepilogo
> "$SUMMARY_FILE"

# Trova tutti i file slowControl*.log e li ordina numericamente
for f in $(ls "${DIR}"/slowControl*.log 2>/dev/null | sort -V); do
    # Estrai l'ultima riga
    last_line=$(tail -n 1 "$f")
    echo "$last_line" >> "$SUMMARY_FILE"
done

cp $SUMMARY_FILE $SUMMARY_DATE

echo "Creato riepilogo: $SUMMARY_DATE"
