import json
 
# Opening JSON file
f = open('Chords.json')
 
# returns JSON object as
# a dictionary
data = json.load(f)
print(data);
