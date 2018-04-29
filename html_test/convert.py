
file = open("page.html", "r")

for line in file.readlines():
    line = line.lstrip()  # remove whitespace
    if not line.startswith("//") and line is not "":  # only use non-comment lines
        print("client.println(\"{}\");\n".format(line[:-1].replace('\"', '\\\"')), end='')

