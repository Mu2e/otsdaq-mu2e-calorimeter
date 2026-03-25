#!/bin/bash

# Cartella contenente i file
DIR="./"  # puoi cambiarlo in un path assoluto

# Nome del file di output
SUMMARY_FILE="summary.log"
SUMMARYVI_FILE="summaryVI.log"
rightnow=$(date +'%Y%m%d_%H%M')
SUMMARY_DATE="summary_${rightnow}.log"
SUMMARYVI_DATE="summaryVI_${rightnow}.log"

# Cancella o crea file riepilogo
> "$SUMMARY_FILE"
> "$SUMMARYVI_FILE"

# Trova tutti i file slowControl*.log e li ordina numericamente
for f in $(ls "${DIR}"/slowControl[0-9]*.log 2>/dev/null | sort -V); do
    # Estrai l'ultima riga
    last_line=$(tail -n 1 "$f")
    echo "$last_line" >> "$SUMMARY_FILE"
done

for f in $(ls "${DIR}"/slowControlVI[0-9]*.log 2>/dev/null | sort -V); do
    # Estrai l'ultima riga
    last_line=$(tail -n 1 "$f")
    echo "$last_line" >> "$SUMMARYVI_FILE"
done

cp $SUMMARY_FILE $SUMMARY_DATE
cp $SUMMARYVI_FILE $SUMMARYVI_DATE

echo "Creato riepilogo: $SUMMARY_DATE, $SUMMARYVI_DATE"
