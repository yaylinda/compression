import sys

def encode(input):

	# create rotations
	num_letters = len(input)
	rotations = []

	for i in range(0, num_letters):
		rotations.append("")

	for i in range(0, num_letters):
		for j in range(0, num_letters):
			index = i+j
			if (index > num_letters-1):
				index = (index % num_letters)
			rotations[i] = rotations[i] + input[index]
	
	# sort rotations
	rotations.sort()

	# find index of original within sorted rotations
	org_index = -1
	for i in range(0, num_letters):
		if rotations[i] == input:
			org_index = i
			break

	# concatonate last letters as string to return
	last_letters = ""
	for i in range(0, num_letters):
		last_letters += rotations[i][num_letters-1]

	return (last_letters, org_index)

def decode(last_letters, org_index):
	
	num_letters = len(last_letters)

	# initialize rotations
	rotations = []
	for i in range(0, num_letters):
		rotations.append("")

	# decoding algorithm
	for i in range(0, num_letters):
		for j in range(0, num_letters):
			rotations[j] = last_letters[j] + rotations[j]
		rotations.sort()

	return rotations[org_index]

def main():
	arg_len = len(sys.argv)
	if arg_len < 4:
		print "Usage: program.py [b OR i] src_filename dest_filename"
	else:
		src = open(sys.argv[2], 'r')
		dest = open(sys.argv[3], 'w')

		# "small" problem for now - can only read/write files that are less than computer's memory
		# also, needs escaping

		# BWT from regular text
		if (sys.argv[1] == 'b'):
			encoded_info = encode(src.read())
			dest.write(encoded_info[0] + "\n" + str(encoded_info[1]))
			print("Done.")

		# inverse BWT to regular text
		elif (sys.argv[1] == 'i'):
			last_letters = src.readline()[:-1]
			org_index = src.readline()
			dest.write(decode(last_letters, int(org_index)))
			print("Done.")
		else:
			print "Error: option not valid."

		src.close()
		dest.close()




















if __name__ == "__main__":
	main()