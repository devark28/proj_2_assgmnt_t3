#!/usr/bin/env python3
import random

random.seed(42)

with open("/usr/share/dict/words") as f:
    words = [w.strip() for w in f if w.strip().isalpha()]

KEYWORD = "kernel"
NUM_FILES = 16
WORDS_PER_FILE = 6000

for i in range(1, NUM_FILES + 1):
    body = random.choices(words, k=WORDS_PER_FILE)

    occurrences = random.randint(0, 12)
    for _ in range(occurrences):
        pos = random.randrange(len(body))
        body[pos] = KEYWORD

    words_per_line = 15
    lines = [" ".join(body[j:j + words_per_line]) for j in range(0, len(body), words_per_line)]

    with open(f"data/file{i:02d}.txt", "w") as out:
        out.write("\n".join(lines) + "\n")

    print(f"data/file{i:02d}.txt: {occurrences} occurrences of '{KEYWORD}'")
