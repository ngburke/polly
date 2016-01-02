#!/usr/bin/python

kbNormal = [
            list("a"),
            list("b"),
            list("c"),
            list("d"),
            list("e"),
            list("f"),
            list("g"),
            list("h"),
            list("i"),
            list("j"),
            list("k"),
            list("l"),
            list("m"),
            list("n"),
            list("o"),
            list("p"),
            list("q"),
            list("r"),
            list("s"),
            list("t"),
            list("u"),
            list("v"),
            list("w"),
            list("x"),
            list("y"),
            list("z"),
            ]

kbStackedAbc = [
             list("a"),
             list("bo"),
             list("cp"),
             list("dq"),
             list("er"),
             list("fs"),
             list("gt"),
             list("hu"),
             list("iv"),
             list("jw"),
             list("kx"),
             list("ly"),
             list("mz"),
             list("n"),
             ] 
 
kbPackedQ = [
             list("qaz"),
             list("wsx"),
             list("edc"),
             list("rfv"),
             list("tgb"),
             list("yhn"),
             list("ujm"),
             list("ik"),
             list("opl"),
             ] 
 
kbFatQ = [
             list("qazwsx"),
             list("edcrfv"),
             list("tgbyhn"),
             list("ujmik"),
             list("opl"),
             ] 

kbPackedA = [
             list("abcd"),
             list("efgh"),
             list("ijkl"),
             list("mnopq"),
             list("rstu"),
             list("vwxyz"),
            ] 

kbDoubleAbc = [
             list("abcdef"),
             list("ghijkl"),
             list("mnopqr"),
             list("stuvwxyz"),
             ] 


def matcher (pressed, checkfor, proximity):
    
    found = False
    
    for i in range(0, len(keyboard)) :
        if pressed in keyboard[i]:
            found = True
            break;
    
    assert found == True
    
    minprox = max(0, i - proximity)
    maxprox = min(i + proximity + 1, len(keyboard))
    
    for i in range(minprox, maxprox) :
        if checkfor in keyboard[i]:
            return True
    
    return False

def typer (stroke, char, typed, words):
    
    full_match = 0
    narrow = []
    full_match_word = ""
    
    if 0 == len(words) :
        return
    
    
    for word in words :
        
        # Make sure we're still in the word boundary
        if stroke >= len(word):
            continue;
        
        # See if the typed character matches the word
        if matcher(char, word[stroke], 0) == True:
            
            if stroke == len(word) - 1:
                # Full word match
                print("\rFull match: " + "(" + "".join(typed) + ") " + str(stroke + 1) + " " + "".join(word), end ="")
                full_match = full_match + 1
                
                
                if full_match > 1 :
                    # We have a collision situation
                    #words.remove(word)
                    if word not in full_collisions:
                        full_collisions.append(word)
                        print("\rCollision: " + "(" + str(len(full_collisions)) + ") " + full_match_word + " -> " + "".join(word))
                else:
                    full_match_word = "".join(word)
                        
                if word not in unique_matches:
                    unique_matches.append(word)
                    strokes_for_match[stroke + 1].append(word)  
                
            # Partial match or full matches are added to the list
            # This is to ensure the next check doesn't think there is a quick match
            # in the situation where a short word is the beginning of another.
            # e.g. full match on 'your', partial match on 'yourself' 
            narrow.append(word)

    if len(narrow) == 1:
        # Quick word match (narrowed down to one possibility early)
        print("\rQuik match: " + "(" + "".join(typed) + ") " + str(stroke + 1) + " " + "".join(narrow[0]) + "                          ", end = "")
        if narrow[0] not in unique_matches:
            unique_matches.append(narrow[0])
            strokes_for_match[stroke + 1].append(narrow[0])  

        return 
                
                
    # Explore depth first
    for char2 in keys_to_press :
        typed.append(char2)
        typer(stroke + 1, char2, typed, narrow)
        typed.pop()
        
    # Explore 1st character breadth
    if stroke == 0 and char != 'z' :
        char = chr(ord(char) + 1)
        typed = list(char)
        typer(stroke, char, typed, words)

wordfile          = 'words_polly.txt'
wordlist          = list(line.strip() for line in open(wordfile))
keyboard          = kbDoubleAbc
#keys_to_press    = string.ascii_lowercase
keys_to_press     = ['a','g','m','s']
full_collisions   = []
unique_matches    = []
strokes_for_match = [[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]]

for i in range(0, 17) :
    strokes_for_match[i] = []

total_matches = 0

first_char = 'a'
typed = [first_char]
typer(0, first_char, typed, wordlist)

print("\n")
    
if len(full_collisions) > 0 :
    print ("WARNING: Collisions: " + str(len(full_collisions)))
    
    for collision in full_collisions :
        wordlist.remove(collision)
    
print("Words found: " + str(len(unique_matches)))
print("Words by strokes to match:")

for i in reversed(range(0, 17)) :
    print("\nMatch in " + str(i) + " (" + str(len(strokes_for_match[i])) + ") :")
    for word in strokes_for_match[i] :
        print("".join(word))
        
        if i >= 7 and len(wordlist) > 2048:
            wordlist.remove(word)
        
print("Wordlist without collisions and long matches (" + str(len(wordlist)) + ")")
print()

##################################################################################

from struct import pack

def get_key_presses(word, length):
    
    value = 0
    
    for char in range(0, min(length, len(word))) :
        for i in range(0, len(kbDoubleAbc)):
            if word[char] in kbDoubleAbc[i]:
                value = value + (i << (char * 2))
                break
        
    return value

# Initialize the arrays

buckets = []
offsets = []

for i in range(0,256) :
    buckets.append([])
    offsets.append([])

# Calculate the buckets based on first four key presses

for word in wordlist:

    # Calculate the hashtable bucket on the first four characters
    bucket = get_key_presses(word, 4)
    
    buckets[bucket].append(word)
    
# Build up the binary output for the word table

EMPTY_HASH_BUCKET = 0xFFFF
max_wordlen       = 0
running_offset    = 0
file_wordtable    = open("c_wordtable.bin", "wb")
file_hashtable    = open("c_hashtable.bin", "wb")

for i in range (0,256):
    
    bucketwords = len(buckets[i])
    
    print ("Bucket " + str(i) + " has " + str(bucketwords) + " words")
    
    if bucketwords > 0:
        offsets[i] = running_offset
    else:
        offsets[i] = EMPTY_HASH_BUCKET
    
    for word in buckets[i] :
        
        print("".join(word))
        
        wordlen = len(word)
        wordbytes = bytes("".join(word), "UTF-8")
        data = pack('<H8s', get_key_presses(word, len(word)), wordbytes)
        
        file_wordtable.write(data)
        
        max_wordlen = wordlen if wordlen > max_wordlen else max_wordlen

        running_offset += 1
        
print()
print("Max wordlen", max_wordlen)

# Build up the binary output for the hash table
    
for i in range(0,256):
    print ("Bucket " + str(i) + " offset " + str(offsets[i]))
    
    data = pack('<H', offsets[i])
    file_hashtable.write(data)
 
