

//Simple class for holding values within the linked list. You'll never see this outside of the list class
class Node {
    private:
        Packet value;
        Node *next = NULL, *back = NULL;
    public:
        
        Node(Packet element) {
          value = element;
        }

       
        Packet getValue() {
            return value;
        }

        Node* getNext() {
            return next;
        }
        Node* getBack() {
            return back;
        }

        void setNext(Node* newNext) {
            next = newNext;
        }
        void setBack(Node* newBack) {
            back = newBack;
        }

        ~Node(){}

};


//linked list class, for making adjustable spaces for holding packets
class LinkedList {
    private:
        int size = 0;
        Node *first = NULL, *last = NULL;
    public:
        //Adds the given packet to the last index
        void add(Packet value) {
            Node* newNode = new Node(value);
            switch(size++) {
                case 0:
                    first = last = newNode;
                    break;
                case 1:
                    last = newNode;
                    first->setNext(last);
                    last->setBack(first);
                    break;
                default:
                    last->setNext(newNode);
                    newNode->setBack(last);
                    last = newNode;
            }
        }

        //Removes and returns the packet first in line.
        //If the list is empty, the packet will have an id of -3
        Packet removeFirst() {
            Packet send;
            if (size == 0) {
                send.content == NULL;
                send.id = -3;
                send.length = 0;
            }
            else {
                send = first->getValue();
                Node* newFirst = first->getNext();
                delete first;

                if (newFirst != NULL) {
                    newFirst->setBack(NULL);
                    first = newFirst;
                }   
            }
            size = size < 1 ? 0 : size-1;
            return send;
        }

        //Returns the first packet without removing it from the list
        Packet peek() {
            if (size == 0){ 
                Packet send;
                send.id = -3;
                return send;
            }
            return first->getValue();
        }


        //Given an array of ids, this function removed any packets that
        //have ids outside of this array, returning the removed packets
        //in a new list (or NULL if nothing was removed)
        LinkedList* removeIfNotPresent(int* ids, int idLength) {
            LinkedList* send = new LinkedList();


            for (int i = 0, max = getSize(); i < max; i++) {
                Packet pack = removeFirst();
                bool remove = true;
                for (int j = 0; remove && j < idLength; j++) {
                    remove = ids[j] != pack.id;
                }

                (remove ? send : this)->add(pack);
            }


            if (send->getSize() == 0) {
                delete send;
                return NULL;
            }
            else {
                return send;
            }
           
        }

        //Lets the user know the index of a packet with the given id
        //Returns the index or -1 if not found.
        int indexOf(int id) {
            Node* node = first;
            int i = 0;
            while (node != NULL && node->getValue().id != id) {
                node = node->getNext();
                i++;
            }
            
            return node == NULL ? -1 : i;
        }


        //Returns the first packet found with the given id
        //Or a dummy packet of -3 ID if not found.
        Packet getByID(int id) {
            Node* node = first;
            while (node != NULL && node->getValue().id != id) {
                node = node->getNext();
            }
            if (node == NULL) {
                Packet send;
                send.id = -3;
                send.content = NULL;
                send.length = 0;
                return send;
            }
            else {
                return node->getValue();
            }
        }


        //Removes every element from the given index and up, destroying them.
        void cutOffAt(int index) {
            Node* node = first;
            for (int i = 0; i < index; i++) {
                node = node->getNext();
            }

            if (node->getBack() != NULL) node->getBack()->setNext(NULL);

            for (int i = index; i < size; i++) {
                Node* next = node->getNext();
                delete node;
                node = next;
            }


            if ((size = index) < 1) first = NULL;
        }

        //Returns the size of the array. Enough said.
        int getSize() {
            return size;
        }

        //Destructor, empties the list.
        ~LinkedList() {
            while (size != 0) {
                removeFirst();
            }
        }
};
