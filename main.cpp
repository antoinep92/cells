#include <map>
#include <vector>
#include <iostream>
#include <cstring>

enum Direction {
	here = 0, left, right, up, down
};

template<class E> struct bi {
	using self = bi<E>;
	E x, y;
};
static const bi<int> directionOffset[5] = {
	{0,0},	// here
	{-1,0},	// left
	{1,0},	// right
	{0,-1},	// up
	{0,1}	// down
};

/*
 *
 * Default		eat food under self
 * Transfer(#)	here = balance
 *				negative = give
 *				positive = take : protection (not efficient), then energy
 * Metabolic(#)	here = mark/com
 *				negative = attack (kills protection more efficiently than take)
 *				positive = clone
 * Move			here = dim/com
 *
 */

struct Action {
	enum Tag { move, eat, clone } const tag;
	Direction direction;
	Action(Tag tag, Direction direction) : tag(tag), direction(direction) {}
};
/// actor moves to an adjacent cell - or dims markers when direction is here
struct Move : Action {
	static const int moveCost	= 10;	///< energy lost for a move
	static const int dimCost	= 1;	///< energy lost for a dim
	static const int dimAmount	= 10;	///< markers lost because of a dim
	static const Tag tag = move;
	Move(Direction dir) : Action(tag, dir) {}
};
/** if direction is here: protect - the entity eat its own protection - or increases it, when amount < 0
 *  else when the cell is free
 *		when amount > 0 : eat - the entity converts available food to energy
 *		when amount < 0 : mark - put markers at given position
 *	else (when there is another entity):
 *		when amount > 0 : the entity transfers the other's protection and then energy to its own energy
 *		when amount < 0 : the entity transfers its energy to the other's protection
 */
struct Eat: DirectedAction {
	static const float	foodSpontaneousIngestion = 1;	///< food ingested at every cycle without action (spontaneously)
	static const float	foodSpontaneousEnergy = .5;		///< (<1) energy gained by spontaneous food ingestion of 1 unit
	static const int	foodMaxIngestion = 10;			///< maximal volontary food ingestion per cycle (in addition to foodSpontaneousIngestion)
	static const float	foodEnergy = .2;				///< (<1) energy gained by volontary food ingestion of 1 unit

	static const float	markerSpontaneousEmission = .01;///< markers emited at every cycle without action (spontaneously)
	static const float	markerSpontaneousEnergy = 10;	///< energy necessary to emit 1 unit of markers (spontaneously)
	static const int	markerMaxEmission = 1;			///< maximal volontary marker emission per cycle (in addtion to markerSpontaneousEmission)
	static const float	markerEnergy = 10;				///< energy necessary to emit 1 unit of markers (volontarily)

	static const float	transferEfficency = 0.9;		///< (<1) efficiency of entity to entity ernegy/protection transfers/attacks
	static const int	transferMaxGive = 1000;			///< maximal ernegy an entity can give in a cycle
	static const int	transferMaxTake = 100;			///< maximal energy an entity can take in a cycle

	static const float spontaneousBalance = foodSpontaneousIngestion * foodSpontaneousEnergy - markerSpontaneousEmission * markerSpontaneousEnergy;

	static const Tag tag = eat;
	int amount;
	Eat(Direction dir, int amount) : Action(tag, dir), amount(amount) {}
};
/** if direction is here: mark - put markers under entity
 *  else
 *		when the cell if free : clone - create a new cell with same DNA
 *		when there is another entity:
 *			if other has same DNA: give - add to its energy/protection as specified
 *			else: infect - change its DNA and substract from its energy/protection
 */
struct Clone : DirectedAction {
	static const Tag tag = clone;
	Clone(Direction dir, int energy, int protection) : DirectedAction(tag, dir), energy(energy), protection(protection) {}
	int energy, protection;
};

struct Pool {
	const int size;
	char * data;
	int offset;
	Pool(int size) : size(size), data(new char[size]), offset(0) {}
	~Pool() { delete[] data; }
	template<class T> T* make() {
		offset += sizeof(T);
		if(offset >= size) throw std::logic_error("Memory pool is fool");
		return (T*)(data + offset);
	}
};


struct AdjacentCell {
	int presence;
	int food;
	std::map<int, int> markers;
};

struct Entity {
	bi<int> pos;
	int energy, protection;
	int dna;
	bool dead() const { return energy <= 0; }
	void protect(int amount) {
		if(amount > energy) { protection += energy; energy = 0; }
		else if(amount < -protection) { energy += protection; protection = 0; }
		else { protection += amount; energy -= amount; }
	}
};

struct Code {
	enum Tag {
		herbivore = 0,
		carnivore,
		lastDeclaredCode
	};
};
struct Herbivore : Code {
	static const Tag tag = herbivore;
	static inline Action* process(Pool & pool, const Entity & entity, const AdjacentCell* adjacency) {
		/// TODO
		return 0;
	}
	static inline float initialProtection() { return 0.8; }
};
struct Carnivore : Code {
	static const Tag tag = carnivore;
	static inline Action* process(Pool & pool, const Entity & entity, const AdjacentCell* adjacency) {
		/// TODO
		return 0;
	}
	static inline float initialProtection() { return 0.2; }
};
struct Dispatcher : Code {
	static inline Action* process(Pool & pool, const Entity & entity, const AdjacentCell* adjacency) {
		switch(entity.code) {
			case herbivore: return Herbivore::process(pool, entity, adjacency);
			case carnivore: return Carnivore::process(pool, entity, adjacency);
			default : throw std::logic_error("bad code tag");
		}
	}
	static inline float initialProtection(int code) {
		switch(code) {
			case herbivore: return Herbivore::initialProtection();
			case carnivore: return Carnivore::initialProtection();
			default : throw std::logic_error("bad code tag");
		}
	}
};

template<int W_, int H_> struct Map {
	static const int W = W_;
	static const int H = H_;
	static const int N = W * H;
	
	static const int moveCost = 10;
	static const int maxFood  = 1000;
	static const int actionSize = 20;
	static const int markerPropagation = 1;
	static const int markerMax = 100;
	static const int markerInc = 1;
	
	using ById = std::map<int, Entity>;
	ById byId;
	Entity** byPos;
	int* food;
	std::map<int, int*> markers;
	ById birthQueue;
	int nextId = 0;
	
	Map() : byPos(new Entity*[N]), food(new int[N]) {
		memset(byPos, 0, sizeof(Entity*) * N);
		memset(food, 0, sizeof(int) * N);
	}
	~Map() {
		delete[] byPos; delete[] food;
		for(auto & m : markers) delete[] m.second;
	}
	
	void add(const Entity & e) {
		const int id = ++nextId;
		const int i  = W * e.pos.y + e.pos.x;
		Entity & ne = byId[id] = e;
		if(byPos[i]) throw std::logic_error("duplicate entity in cell");
		byPos[i] = &ne;
	}
	template<class F> void forEachInt(int* data) { for(int i = 0; i < N; ++i) F::f(data[i]); }
	void regenFood() {
		struct F { static inline void f(int & x) { if(x < maxFood) ++x; } };
		forEachInt<F>(food);
		
	}
	
	int count() const { return byId.size(); }
	
	void buildAdjacency(AdjacentCell* adjacency, int i) {
		adjacency->presence = byPos[i] ? Dispatcher::marker(byPos[i]->code) : 0; /// TODO: optimize for [here]
		adjacency->food = food[i];
		adjacency->markers.clear();
		for(auto & marker : markers) adjacency->markers[marker.first] = marker.second[i];
	}
	void buildAdjacency(const bi<int> & pos, AdjacentCell* adjacency) {
		buildAdjacency(adjacency, pos.x + pos.y * W);		// here
		buildAdjacency(adjacency+1, bitoi(pos.x-1, pos.y));	// left
		buildAdjacency(adjacency+2, bitoi(pos.x+1, pos.y));	// right
		buildAdjacency(adjacency+3, bitoi(pos.x, pos.y-1));	// up
		buildAdjacency(adjacency+4, bitoi(pos.x, pos.y+1));	// down
	}
	
	void actions(Pool & pool, AdjacentCell* adjacency, int id, Entity & entity) {
			buildAdjacency(entity.pos, adjacency);
			Action* action = Dispatcher::process(pool, iter->second, adjacency);
			if(action) {
				switch(action->tag) {
					case balance: {
						const int amount = reinterpret_cast<Balance*>(action)->amount;
						if(amount > entity.protection) amount = entity.protection;
						else if(amount < -entity.energy)
						entity.energy += 
						break;
					}
					case move:
						break;
					case transfer:
						break;
					case clone:
						break;
				}
			}
			if(cell.second.enregy <= 0) {
				
			}
		}
	}
	void actions() {
		Pool pool(actionSize * count());
		AdjacentCell adjacency[5];
		// ensure relative fairness by alternating scan order
		if(rand() & 1)	for(auto iter = byId.begin(); iter != byId.end(); ++iter) actions(pool, adjacency, iter->first, iter->second);
		else			for(auto iter = byId.rbegin(); iter != byId.rend(); ++iter) actions(pool, adjacency, iter->first, iter->second);
	}
	
	void decayMarkers() {
		struct F { static inline void f(int & x) { if(x) --x; } };
		for(auto & m : markers) forEachInt<F>(m.second);
	}
	void diffuseMarkers() {
		
	}
	void placeMarkers() {
		for(auto & p : byPos) limit<markerMax>(getMarker(p.second.code, p.second.pos)) += markerInc;
	}
	void doMarkers() {
		decayMarkers();		// with time, markers disapear
		diffuseMarkers();	// markers are diffused using a conservative convolution
		placeMarkers();		// entities emit markers until saturation
	}
	
	void cycle() {
		doMakers();
		actions();
		regenFood();
	}
	
	static inline int bitoi(int x, int y) {
		if(x < 0) x += W;
		if(x >= W) x -= W;
		if(y < 0) y += H;
		if(y >= H) y -= H;
		return x + y * W;
	}
	Entity* operator()(const bi<int> & pt) {
		return byPos[pt.y * W + pt.x];
	}
	Entity* operator()(int id) {
		auto iter = byId.find(id);
		if(iter == byId.end()) return 0;
		return &iter->second;
	}
	
};

struct Init {
	static const int initial = 100;
	template<int W, int H> void operator()(Map<W, H> & map) {
		for(int i = 0; i < W*H; ++i) map.food[i] = rand() % 100;
		for(int code = 0; code < Code::lastDeclaredCode; ++code) {
			const int n = (W * H) / 50;
			for(int i = 0; i < n; ++i) {
				bi<int> pt;
				do pt = bi<int>{ rand() % W, rand() % H };
				while(map(pt));
				const float balance = Dispatcher::initialBalance(code);
				map.add({ pt, int(initial * balance), int(initial * (1-balance)), code });
			}
		}
	}
};


int main() {
	using std::cout; using std::endl;
	Map<1000, 1000> map;
	cout << "map created" << endl;
	Init init; init(map);
	cout << "map initialized" << endl;
	int cycle = 0;
	while(true) {
		map.cycle();
		std::cout << ++cycle << '\t' << map.count() << std::endl;
	}
	return 0;
}
