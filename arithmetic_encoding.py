import sys
import math

# global variables used for compression
precision = 32
whole = math.pow(2, precision)
half = whole/2
quarter = whole/4

def setup_compress(src_filename):
	src = open(src_filename, 'r')
	input_stream = src.read().split()

	counts = {}
	pmf = {}
	cdf = {}
	cdf2 = {}
	total_count = 0

	# get counts of words
	for word in input_stream:
		if (word not in counts.keys()):
			counts[word] = 1
		else:
			counts[word] = counts[word] + 1

	# get total word count
	for word in counts.keys():
		total_count += counts[word]

	# normalize counts to get pmf
	for word in counts.keys():
		pmf[word] = counts[word]*1.0 / total_count

	# calculate cdf vector
	previous_prob = 0
	for word in counts.keys():
		cdf[word] = previous_prob
		previous_prob += counts[word]

	# caluclate cdf2 vector
	for word in counts.keys():
		cdf2[word] = cdf[word] + counts[word]

	src.close()

	return (cdf, cdf2, total_count)

def setup_decompress(src_filename):
	src = open(src_filename, 'r')

	encoded_string = src.readline()[:-1]
	cdf = eval(src.readline()[:-1])
	cdf2 = eval(src.readline()[:-1])
	total_count = int(src.readline())

	src.close()

	return (encoded_string, cdf, cdf2, total_count)

def compress(src_filename, dest_filename, cdf, cdf2, total_count):
	src = open(src_filename, 'r')
	input_stream = src.read().split()

	a = 0
	b = whole
	s = 0
	encoded_string = ""

	for word in input_stream:
		diff = b - a
		b = a + round((diff * cdf2[word]) / total_count)
		a = a + round((diff * cdf[word]) / total_count)

		while (b < half or a > half):
			if (b < half):
				encoded_string += "0" + "1"*s
				s = 0
				a = 2*a
				b = 2*b
			elif (a > half):
				encoded_string += "1" + "0"*s
				s = 0
				a = 2 * (a - half)
				b = 2 * (b - half)
		while (a > quarter and b < 3*quarter):
			s += 1
			a = 2 * (a - quarter)
			b = 2 * (b - quarter)

	s += 1
	if (a <= quarter):
		encoded_string += "0" + "1"*s
	else:
		encoded_string += "1" + "0"*s

	# print "encoded_string: " + encoded_string

	# write info to file
	dest = open(dest_filename, 'w')
	dest.write(encoded_string+"\n")
	dest.write(str(cdf)+"\n")
	dest.write(str(cdf2)+"\n")
	dest.write(str(total_count)+"\n")

	src.close()
	dest.close()

def decompress(dest_filename, encoded_string, cdf, cdf2, total_count):
	a = 0
	b = whole
	z = 0
	decoded_string = ""

	index = 0
	while (index < precision and index < len(encoded_string)):
		if (encoded_string[index] == "1"):
			z += math.pow(2, precision-(index+1))
		index += 1

	# print "z: ", z

	for i in range(0, total_count):
		print i, "out of", total_count
		# print "index:", index
		# print "z:", z
		# print "old lower:", a
		# print "old upper:", b
		for word in cdf.keys():
			# print "\tword:", word
			diff = b - a
			b0 = a + round((diff * cdf2[word]) / total_count)
			a0 = a + round((diff * cdf[word]) / total_count)
			# print "\tnew lower:", a0
			# print "\tnew upper:", b0
			if (a0 <= z and z < b0):
				a = a0
				b = b0
				decoded_string += word + " "
				# print "\tfound symbol! ", word
				break;
			# print ""
		while (b < half or a > half):
			if (b < half):
				# print "\tb<half"
				a = 2 * a
				b = 2 * b
				z = 2 * z
			elif (a > half):
				# print "\ta>half"
				a = 2 * (a - half)
				b = 2 * (b - half)
				z = 2 * (z - half)
			else:
				print "ERROR: should not be here."

			if (index <= len(encoded_string)-1):
				if (encoded_string[index] == "1"):
					z += 1
			index += 1
		while (a > quarter and b < 3*quarter):
			# print ""
			a = 2 * (a - quarter)
			b = 2 * (b - quarter)
			z = 2 * (z - quarter)
			if (index <= len(encoded_string)-1):
				if (encoded_string[index] == "1"):
					z += 1
			index += 1

	# print "decoded_string: " + decoded_string

	# write decoded_string to file
	dest = open(dest_filename, 'w')
	dest.write(decoded_string)

	dest.close()

def verify(src_filename, dest_filename):
	file1 = open(src_filename, 'r')
	input_stream1 = file1.read().split()

	file2 = open(dest_filename, 'r')
	input_stream2 = file2.read().split()

	if (len(input_stream1) != len(input_stream2)):
		print "file 1 length: ", len(input_stream1)
		print "file 2 length: ", len(input_stream2)
		return False

	for i in range(0, len(input_stream1)):
		if (input_stream1[i] != input_stream2[i]):
			print "failed on word #", i
			return False

	file1.close()
	file2.close()

	return True

def main():
	arg_len = len(sys.argv)
	if arg_len < 4:
		print "Usage: program.py [c OR d OR v] src_filename dest_filename"
	else:
		src_filename = sys.argv[2]
		dest_filename = sys.argv[3]

		# compress
		if (sys.argv[1] == 'c'):
			(cdf, cdf2, total_count) = setup_compress(src_filename)
			# print cdf
			# print cdf2
			# print total_count
			compress(src_filename, dest_filename, cdf, cdf2, total_count)
			print "Done."
		# decompress
		elif (sys.argv[1] == 'd'):
			(encoded_string, cdf, cdf2, total_count) = setup_decompress(src_filename)
			# print encoded_string
			# print cdf
			# print cdf2
			# print total_count
			decompress(dest_filename, encoded_string, cdf, cdf2, total_count)
			print "Done."
		# compare two files
		elif (sys.argv[1] == 'v'):
			result = verify(src_filename, dest_filename)
			if (result):
				print "PASS."
			else:
				print "FAIL."
		# error
		else:
			print "Usage: program.py [c OR d OR v] src_filename dest_filename"

if __name__ == "__main__":
	main()
