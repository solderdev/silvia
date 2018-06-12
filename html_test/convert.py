
file = open("index.html", "r")

for line in file.readlines():
    line = line.lstrip()  # remove whitespace
    if not line.startswith("//") and line is not "":  # only use non-comment lines
        print("cl.println(\"{}\");\n".format(line[:-1].replace('\"', '\\\"')), end='')

