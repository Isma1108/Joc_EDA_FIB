#include "Player.hh"
#include <queue>
#include <vector>
#include <map>
#include <set>

/*
  La estrategia que planteo es relativamente sencilla. El objetivo está en asignar una tarea a cada unidad viva de mi equipo,
  de forma que (por ejemplo) 2 mismas unidades no vayan a por una misma comida. Busco repartir el trabajo y hacer que más de
  una unidad vaya a cierto punto de interés solo cuando lo considere estrictamente necesario. Inicialmente recorro todas mis
  unidades vivas para poder ver si alguna se encuentra en una "situación extrema" donde deberá hacer un movimiento determinado
  en base a ciertos criterios (se pueden ver en los comentarios de las funciones). Si la unidad se tiene que mover, inserto en 
  un map la pareja id_unidad - movimiento + la prioridad de ese movimiento (a cont. explicaré porqué la prioridad). Paralelamente
  a este recorrido inicial, para las unidades que no estan en situación extrema, voy poniendo a "waste imaginario" de la matriz
  que creo en cada ronda aquellas posiciones que impliquen ser mordido por un zombie si voy allí. Al salir de este primer recorrido,
  hago un BFS desde todos los puntos de interés (cadáver, zombie, comida, enemigo) e inserto en una cola de prioridad las parejas 
  punto_de_interés - id_unidad, donde la prioridad va a depender del tipo de punto de interés y la distancia hasta la unidad, entre
  otras cosas. Cabe destacar que el BFS no va a tener en cuenta celdas que impliquen ser mordido por un zombie (precisamente por
  el waste imaginario de mi tablero, hecho en el recorrido anterior). Una vez acabados los BFS des de todos los puntos de interés,
  voy sacando de la cola los elementos, y por ser de prioridad los sacaré en orden óptimo. Si al sacar un elemento veo que, o la unidad
  ya tiene una tarea, o el puntos de interés ya tiene una unidad asignada, simplemente lo ignoro. Si veo que no hay conflicto alguno,
  inserto en el map la pareja id_unidad - movimiento. Al acabar de sacar todos los elementos de la cola, rerecorro mis unidades, si 
  tienen tarea a hacer (una entrada en el map) hago push en otra cola de prioridad la pareja id_unidad - movimiento donde la prioridad es
  una u otra en función de qué debe hacer la unidad (por ejemplo, es preferible hacer mas tarde un movimiento hacia una celda donde
  un enemigo también puede ir, para aprovechar el 30% de ataque si él se mueve antes). Si la unidad no tiene nada que hacer, se inicia un
  BFS con profundidad limitada para buscar una celda sin owner o enemiga, y en caso de no estar suficientemente cerca simplemente se
  queda quieta (para controlar mínimamente las zonas) Pero OJO!, las unidades que no se mueven también se pushean a la cola con un 
  movimiento ILEGAL, para hacer que las cosas que se quieren hacer tarde se hagan aún mas tarde. Una vez acabado este recorrido sólo 
  queda ir sacando los elementos de esa última cola e ir realizándolos.
*/

/**
 * Write the name of your player and save this file
 * with the same name and .cc extension.
 */
#define PLAYER_NAME MegaTorino

struct PLAYER_NAME : public Player {

  /**
   * Factory: returns a new instance of this class.
   * Do not modify this function.
   */
  static Player* factory () {
    return new PLAYER_NAME;
  }

  /**
   * Types and attributes for your player can be defined here.
   */
  
  //Una celda no puede tener algo que no sea:
  enum Content {
    Enemy,
    Corpse,
    Zombie,
    Friend,
    Food,
    Wst,
    F_Street,
    E_Street
  };
  

  #define ID int 

  //4 niveles de prioridad
  #define MIN_PRIO 0
  #define MID_PRIO 1
  #define MAX_PRIO 2
  #define SUPER_PRIO 3

  //Definición de tipos
  typedef vector<int> VI;
  typedef vector<VI> VVI;
  typedef vector<bool> VB;
  typedef vector<VB> VVB;
  typedef vector<CellType> VT;
  typedef vector<VT> VVT;
  typedef vector<Content> VC;
  typedef vector<VC> VVC;
  typedef vector<Dir> VD;
  typedef pair<int,int> PI;
  typedef pair<int,Dir> PID;
  typedef pair<int,PID> order;
  typedef pair<int, pair<ID, pair<Pos, Dir>>> task;
  typedef set<Pos>::const_iterator set_pos_it;
  typedef priority_queue<task, vector<task>, greater<task>> TK;

  //Variables globales:
  VVC cnt;  //Matriz que se actualiza en cada ronda
  bool crazy_mode = false; //Modo "loco", se activa cuando quedan pocas rondas, o cuando una unidad se infecta
  
  //El vector de direcciones será permutado aleatoriamente en cada ronda.
  vector<Dir> dirs = {Up, Down, Left, Right};
  const vector<Dir> fixed_dirs = {Up, Down, Left, Right};

  const Dir ILEGAL_MOVE = UL; //El movimiento ILEGAL :)
  int MAX_PROF = 50;          //La profundidad máxima del BFS desde puntos de interés, será esta en caso de que no consumamos mucha CPU...


  //********FUNCTIONS********

  //Lectura del contenido de todas las celdas en una matriz.
  void read_content_data(set<Pos>& interest_points) {
    cnt.resize(board_rows());
    for (int i = 0; i < board_rows(); ++i) {
      cnt[i].resize(board_cols());
      for (int j = 0; j < board_cols(); ++j) {
        Cell c_aux = cell(i,j);
        if (c_aux.food) {cnt[i][j] = Food; interest_points.insert(Pos(i,j));}
        else if (c_aux.id != -1) {
          int owner_id = unit(c_aux.id).player;
          if (unit(c_aux.id).type == Dead) {cnt[i][j] = Corpse; interest_points.insert(Pos(i,j));}
          else if (owner_id == me()) cnt[i][j] = Friend;
          else if (owner_id != -1) {cnt[i][j] = Enemy; interest_points.insert(Pos(i,j));}
          else {cnt[i][j] = Zombie; interest_points.insert(Pos(i,j));}
        }
        else if (c_aux.type == Waste) cnt[i][j] = Wst;
        else if (c_aux.owner == me()) cnt[i][j] = F_Street;
        else cnt[i][j] = E_Street;
      }
    }
  }

  //Se hace una permutation aleatoria del vector de direcciones.
  void random_dirs() {
    vector<int> v = random_permutation(4);
    dirs[0] = fixed_dirs[v[0]];
    dirs[1] = fixed_dirs[v[1]];
    dirs[2] = fixed_dirs[v[2]];
    dirs[3] = fixed_dirs[v[3]];
  }
  
  //Devuelve true si un zombie puede morder a la unidad en la posición p
  bool zombie_will_kill_me(const Pos& p) {
    int x = p.i, y = p.j;
    int lim_x = int(cnt.size()-1);
    int lim_y = int(cnt[0].size()-1);

    if (x > 0 and cnt[x-1][y] == Zombie) return true;
    if (x < lim_x and cnt[x+1][y] == Zombie) return true;
    if (y > 0 and cnt[x][y-1] == Zombie) return true;
    if (y < lim_y and cnt[x][y+1] == Zombie) return true;
    
    if (x > 0 and y > 0 and cnt[x-1][y-1] == Zombie) return true;
    if (x < lim_x and y > 0 and cnt[x+1][y-1] == Zombie) return true;
    if (x > 0 and y < lim_y and cnt[x-1][y+1] == Zombie) return true;
    if (x < lim_x and y < lim_y and cnt[x+1][y+1] == Zombie) return true;

    return false;
  }

  //Devuelve true si la unidad en la posición p tiene un zombie en diagonal.
  bool zombie_in_diagonal(const Pos& p) {
    int x = p.i, y = p.j;
    int lim_x = int(cnt.size()-1);
    int lim_y = int(cnt[0].size()-1);
    if (x > 0 and y > 0 and cnt[x-1][y-1] == Zombie) return true;
    if (x < lim_x and y > 0 and cnt[x+1][y-1] == Zombie) return true;
    if (x > 0 and y < lim_y and cnt[x-1][y+1] == Zombie) return true;
    if (x < lim_x and y < lim_y and cnt[x+1][y+1] == Zombie) return true;
    return false;
  }
  
  //Pone a "waste imaginario" las celdas adyacentes a p que impliquen ser mordido por un zombie
  void waste_danger_cells(const Pos& p) {
    if (zombie_will_kill_me(p)) cnt[p.i][p.j] = Wst;
    for (Dir d : fixed_dirs) {
      if (pos_ok(p+d) and zombie_will_kill_me(Pos(p+d))) {
        cnt[Pos(p+d).i][Pos(p+d).j] = Wst; 
      }
    }
  }

  //Devuelve true si un enemigo es adyacente a la posición p (por ende puede ir)
  bool enemy_will_go(const Pos& p) {
    for (Dir d : fixed_dirs) {
      Pos p_aux = Pos(p+d);
      if (pos_ok(p_aux) and cnt[p_aux.i][p_aux.j] == Enemy) return true;
    }
    return false;
  }
 
  //Devuelve true si hay un cadáver adyacente a la celda en la posición p
  bool some_corpse_adj(const Pos& p) {
    for (Dir d : fixed_dirs) {
      Pos pos_aux = Pos(p+d);
      if (pos_ok(pos_aux) and cnt[pos_aux.i][pos_aux.j] == Corpse) return true;
    }
    return false;
  }
 
  //Devuelve true si hay un enemigo adyacente a la posición p, y deja en penem la posición del enemigo
  bool some_enemy_adj(const Pos& p, Pos& penem) {
    for (Dir d : fixed_dirs) {
      Pos pos_aux = Pos(p+d);
      if (pos_ok(pos_aux) and cnt[pos_aux.i][pos_aux.j] == Enemy) {
        penem = pos_aux;
        return true;
      }
    }
    return false;
  }
  
  //Devuelve true si hay un enemigo adyacente a la posición p
  bool some_enemy_adj(const Pos& p) {
    for (Dir d : fixed_dirs) {
      Pos pos_aux = Pos(p+d);
      if (pos_ok(pos_aux) and cnt[pos_aux.i][pos_aux.j] == Enemy) {
        return true;
      }
    }
    return false;
  }
  
  //Devuelve true si o la unidad con ID id ya tiene tarea o la posición p (con punto de interés) ya tiene unidad asignada
  bool already_assigned(ID id, const Pos& p, const set<ID>& units, const set<Pos>& pos) {
    set<ID>::const_iterator it1 = units.find(id);
    set<Pos>::const_iterator it2 = pos.find(p);
    if (it1 != units.end() or it2 != pos.end()) return true;
    return false;
  }

  //Devuelve la prioridad de una pareja punto_de_interés - unidad a meter en la cola de prioridad de tareas
  int decide_prio(const Content& c, int depth, Dir& d, const Pos& p, ID id) {
    //Si no quedan pocas rondas y la unidad no está infectada ...
    if (not crazy_mode and unit(id).rounds_for_zombie == -1) {
      if (c == Food) return depth;  //La comida es lo mas prioritario!!
      if (c == Corpse) {
        return 4 + depth;
      }
      if (c == Enemy) {
        if (depth == 3) d = ILEGAL_MOVE; //Si estoy a dos celdas de un enemigo, me quedo quieto para favorecer mi búsqueda del 30% :)
                                         //Él se va a mover, podremos estar a una celda y allí entran mis prioridades en juego!

        //En función del número de unidades del rival, me quedaré quieto para hacer un ataque con máxima prioridad,
        //o iré a por él con prioridad mínima.
        else if (depth == 2 and alive_units(cell(p).owner).size() > alive_units(me()).size()) d = ILEGAL_MOVE;

        //Atacaré antes a un enemigo más flojo que yo
        int dif_strength = strength(me()) - strength(cell(unit(id).pos).owner);
        if (dif_strength > 0) return 6 + depth;
        return 6 + depth;
      }
      return 4 + depth;
    }
    //En caso contrario sólo importa la distancia (un infectado debe ir a lo más cercano)
    else {
      //Si es un cadáver, hay que sumarle el tiempo de conversion
      if (c == Corpse) {
        return unit(cell(p).id).rounds_for_zombie + depth;
      }
      return depth;
    }
  }

  //Invierte la dirección d
  void reverse_dir(Dir& d) {
    if (d == Up) d = Down;
    else if (d == Down) d = Up;
    else if (d == Left) d = Right;
    else d = Left;
  }

  //BFS desde los puntos de interés
  void BFS_from_ipoints(const Pos& p, TK& tasks) {
    Content c = cnt[p.i][p.j];
    VVB visited(cnt.size(), VB(cnt[0].size(), false));
    queue<pair<Dir,PI>> Q;
    int cells = 0;
    
    visited[p.i][p.j] = true;
    int x = p.i, y = p.j;
 
    for (int i = 0; i < int(dirs.size()); ++i) {
      if (dirs[i] == Up and x > 0 and cnt[x-1][y] != Wst and cnt[x-1][y] != Corpse) Q.push(make_pair(Up, make_pair(x-1, y)));
      if (dirs[i] == Right and y < int(cnt[0].size()-1) and cnt[x][y+1] != Wst and cnt[x][y+1] != Corpse) Q.push(make_pair(Right, make_pair(x, y+1)));
      if (dirs[i] == Down and x < int(cnt.size()-1) and cnt[x+1][y] != Wst and cnt[x+1][y] != Corpse) Q.push(make_pair(Down, make_pair(x+1, y)));
      if (dirs[i] == Left and y > 0 and cnt[x][y-1] != Wst and cnt[x][y-1] != Corpse) Q.push(make_pair(Left, make_pair(x, y-1)));
    }
    
    //Controlar el nivel en el que estoy es sencillo, solo hace falta calcular el size de la cola, cuando es 0 he canviado de nivel (y vuelvo a cargar con el nuevo size)
    int depth = 1;
    while (not Q.empty() and depth < MAX_PROF) {
      int level_size = Q.size(); 

      while (level_size-- != 0) {
        int x = Q.front().second.first;
        int y = Q.front().second.second;
        Dir d = Q.front().first;
        Q.pop();
       

        //Si me encuentro un aliado ...
        if (cnt[x][y] == Friend) {
          ID id = cell(x,y).id;
          reverse_dir(d); //El movimiento que debe hacer el aliado és el inverso del movimiento con el que llega el BFS
          int prio = decide_prio(c, depth, d, p, id); //Decido la prioridad de la pareja;
          tasks.push(make_pair(prio, make_pair(id, make_pair(p, d))));  //La pusheo a la cola de tareas
        }
        
        if (not visited[x][y]) {
          visited[x][y] = true;
          for (int i = 0; i < int(dirs.size()); ++i) {
            if (dirs[i] == Up and x > 0 and cnt[x-1][y] != Wst and cnt[x-1][y] != Corpse) Q.push(make_pair(Up, make_pair(x-1, y)));
            if (dirs[i] == Right and y < int(cnt[0].size()-1) and cnt[x][y+1] != Wst and cnt[x][y+1] != Corpse) Q.push(make_pair(Right, make_pair(x, y+1)));
            if (dirs[i] == Down and x < int(cnt.size()-1) and cnt[x+1][y] != Wst and cnt[x+1][y] != Corpse) Q.push(make_pair(Down, make_pair(x+1, y)));
            if (dirs[i] == Left and y > 0 and cnt[x][y-1] != Wst and cnt[x][y-1] != Corpse) Q.push(make_pair(Left, make_pair(x, y-1)));
          }
        }
      }
      ++depth;
    }
  }

  //BFS hacia una celda enemiga o sin owner, devuelve true si se ha encontrado una hasta la profundidad límite, false en caso contrario
  bool BFS_to_EStreet(const Pos& p, Dir& move_to_do) {
    VVB visited(cnt.size(), VB(cnt[0].size(), false));
    queue<pair<Dir,PI>> Q;
    
    visited[p.i][p.j] = true;
    int x = p.i, y = p.j;

    for (int i = 0; i < int(dirs.size()); ++i) {
      if (dirs[i] == Up and x > 0 and cnt[x-1][y] != Wst and cnt[x-1][y] != Friend) Q.push(make_pair(Up, make_pair(x-1, y)));
      if (dirs[i] == Right and y < int(cnt[0].size()-1) and cnt[x][y+1] != Wst and cnt[x][y+1] != Friend) Q.push(make_pair(Right, make_pair(x, y+1)));
      if (dirs[i] == Down and x < int(cnt.size()-1) and cnt[x+1][y] != Wst and cnt[x+1][y] != Friend) Q.push(make_pair(Down, make_pair(x+1, y)));
      if (dirs[i] == Left and y > 0 and cnt[x][y-1] != Wst and cnt[x][y-1] != Friend) Q.push(make_pair(Left, make_pair(x, y-1)));
    }

    int depth = 1;
    bool limit_depth = false, found = false;
    while (not Q.empty() and not limit_depth and not found) {
      int level_size = Q.size(); 
      while (level_size-- != 0 and not found) {
        int x = Q.front().second.first;
        int y = Q.front().second.second;
        Dir d = Q.front().first;
        Q.pop();
        if (cnt[x][y] == E_Street) {
          move_to_do = d;
          found = true;
        }
        
        if (not visited[x][y] and not found) {
          visited[x][y] = true;
          for (int i = 0; i < int(dirs.size()); ++i) {
            if (dirs[i] == Up and x > 0 and cnt[x-1][y] != Wst and cnt[x-1][y] != Friend) Q.push(make_pair(d, make_pair(x-1, y)));
            if (dirs[i] == Right and y < int(cnt[0].size()-1) and cnt[x][y+1] != Wst and cnt[x][y+1] != Friend) Q.push(make_pair(d, make_pair(x, y+1)));
            if (dirs[i] == Down and x < int(cnt.size()-1) and cnt[x+1][y] != Wst and cnt[x+1][y] != Friend) Q.push(make_pair(d, make_pair(x+1, y)));
            if (dirs[i] == Left and y > 0 and cnt[x][y-1] != Wst and cnt[x][y-1] != Friend) Q.push(make_pair(d, make_pair(x, y-1)));
          }
        }
      }
      ++depth;
      if (depth > 20) limit_depth = true; 
    }
    return found; 
  }

  //Devuelve un entero que indica cuánto merece la pena atacar al enemigo enemy (como mínimo 1, nunca NO atacaré a un rival
  //por ser mas flojo, como mucho no lo atacaré porqué tengo algo mas prioritario que hacer).
  int worth_atack(int me, int enemy) {
    int s_me = strength(me), s_enemy = strength(enemy);
    if (s_me > s_enemy) return s_me - s_enemy + 1; 
    else return 1;
  }
 
  //Devuelve true si hay una celda accesible desde la posición p donde un zombie no me pueda morder, y deja en move_to_do el
  //movimiento que hay que hacer para llegar a ella, false en caso contrario
  bool safe_cell(const Pos& p, Dir& move_to_do) {
    priority_queue<PID> pq;
    for (Dir d : fixed_dirs) {
      int i = Pos(p+d).i, j = Pos(p+d).j;
      if (pos_ok(p + d) and not zombie_will_kill_me(Pos(p+d))) {
        if (cnt[i][j] != Enemy and cnt[i][j] != Wst and cnt[i][j] != Corpse) {
          if (cnt[i][j] == Food) pq.push(make_pair(1,d));
          else pq.push(make_pair(0,d));
        }
      }
    }
    if (not pq.empty()) {move_to_do = pq.top().second; return true;}
    else return false;
  }

  //Devuelve true si hay un enemigo que me puede robar el zombie que saldrá de un cadáver (fijarse que es una adyacencia concreta).
  //Se deduce que "campeo" los cadáveres, ya que se ganan muchos puntos matando zombies.
  bool someone_will_rob_me(const Pos& p, Dir& move_to_do, Pos& ip) {
    for (Dir d : fixed_dirs) {
      Pos pos_aux = Pos(p+d);
      int i = pos_aux.i, j = pos_aux.j;
      Pos e;
      if (pos_ok(pos_aux) and cnt[i][j] != Corpse and cnt[i][j] != Wst and some_enemy_adj(pos_aux, e)) {
        ip = e;
        move_to_do = d;
        return true;
      }   
    }
    return false;
  }

  //Devuelve true si el jugador en la posición p se encuentra en una situación extrema, actualizando las variables pasadas 
  //por referencia correspondientes, false en caso contrario.
  bool extreme_situation(const Pos& p, Pos& ip, Dir& move_to_do, int& prio, bool& ip_ocuped) {
    int i = p.i, j = p.j;
    VD enemy_dirs(0);
    VD zombie_dirs(0);
    VD corpse_dirs(0);
    
    //Primero lleno los vector con los zombies, enemigos o cadáveres adyacentes:
    if (i > 0) {
      if (cnt[i-1][j] == Enemy) enemy_dirs.push_back(Up);
      else if (cnt[i-1][j] == Zombie) zombie_dirs.push_back(Up);
      else if (cnt[i-1][j] == Corpse) corpse_dirs.push_back(Up);
    }
    if (i < int(cnt.size() - 1)) {
      if (cnt[i+1][j] == Enemy) enemy_dirs.push_back(Down);
      else if (cnt[i+1][j] == Zombie) zombie_dirs.push_back(Down);
      else if (cnt[i+1][j] == Corpse) corpse_dirs.push_back(Down);
    }
    if (j > 0) {
      if (cnt[i][j-1] == Enemy) enemy_dirs.push_back(Left);
      else if (cnt[i][j-1] == Zombie) zombie_dirs.push_back(Left);
      else if (cnt[i][j-1] == Corpse) corpse_dirs.push_back(Left);
    }
    if (j < int(cnt[0].size()-1)) {
      if (cnt[i][j+1] == Enemy) enemy_dirs.push_back(Right);
      else if (cnt[i][j+1] == Zombie) zombie_dirs.push_back(Right);
      else if (cnt[i][j+1] == Corpse) corpse_dirs.push_back(Right);
    }

    //Si solo tengo cadáveres adyacentes
    if (enemy_dirs.size() == 0 and zombie_dirs.size() == 0 and corpse_dirs.size() > 0) {
      //Si un zombie me puede matar busco una safe_cell.
      int rounds_zombie = unit(cell(p).id).rounds_for_zombie;
      if (rounds_zombie == -1 and zombie_will_kill_me(p)) {
        if (safe_cell(p, move_to_do)) {
          ip_ocuped = false;
          //Si un enemigo puede ir allí, minima prioridad para aprovechar el 30%, si no prioridad media.
          if (some_enemy_adj(p+move_to_do)) prio = MIN_PRIO;
          else prio = MID_PRIO;
          return true;
        }
        //Si no hay safe_cell, intento ver si hay algun enemigo que me pueda robar para almenos llevarme los puntos matándolo (prioridad mínima)
        else if (someone_will_rob_me(p, move_to_do, ip)) {
          prio = MIN_PRIO;
          return true;
        }
        //Si no, me quedo quieto con prioridad media :(
        else {
          move_to_do = ILEGAL_MOVE;
          prio = MID_PRIO;
          ip = p + corpse_dirs[random(0,corpse_dirs.size()-1)];
          return true;
        }
      }
    }
    else {
      //Si tengo enemigos adyacentes:
      if (enemy_dirs.size() > 0) {
        //Si solo tengo un enemigo adyacente lo ataco a él
        if (enemy_dirs.size() == 1) {
            move_to_do = enemy_dirs[0];
            ip = p + enemy_dirs[0];
            //Si está en juego el zombie de un cadáver super prioridad, si no, solo prioridad máxima.
            if (some_corpse_adj(p)) prio = SUPER_PRIO;
            else prio = MAX_PRIO;
            return true;
        }
        //Si tengo mas de un enemigo adyacente, escojeré el más débil para atacarlo con máxima prioridad
        else if (enemy_dirs.size() > 1) {
          PI final_dir;
          final_dir.first = 0;
          final_dir.second = worth_atack(me(), cell(p + enemy_dirs[0]).owner);
          for (int i = 1; i < int(enemy_dirs.size()); ++i) {
            int worth = worth_atack(me(), cell(p + enemy_dirs[i]).owner);
            if (worth > final_dir.second) final_dir = make_pair(i, worth); 
          }
          move_to_do = enemy_dirs[final_dir.first];
          ip = p + enemy_dirs[final_dir.first];
          prio = MAX_PRIO; 
          return true;
        }
      }
      //Si solo tengo zombies adyacentes
      else if (zombie_dirs.size() > 0) {
        //Si no estoy infectado, y tengo un zombie en diagonal pero me puedo salvar, escapo con prioridad media.
        if (unit(cell(p).id).rounds_for_zombie == -1 and zombie_in_diagonal(p) and safe_cell(p, move_to_do)) {
          ip_ocuped = false; 
          prio = MID_PRIO;
          return true;
        }
        //Si no, ataco a cualquiera de los zombies adyacentes con prioridad media
        Pos aux;
        int i = random(0,zombie_dirs.size()-1);
        move_to_do = zombie_dirs[i];
        ip = p + zombie_dirs[i];
        if (some_enemy_adj(p + zombie_dirs[i], aux)) prio = MAX_PRIO; 
        else prio = MID_PRIO; 
        return true;
      }
    }
    return false;
  }
  
  void move_units() {
    
    set<Pos> interest_points;
    set<ID> units_assigned;
    set<Pos> pos_assigned;
    map<ID,pair<Dir,int>> final_tasks;

    //Leo el contenido de mi matriz de contenido
    read_content_data(interest_points);
    
    //Hago una permutatión aleatoria del vector de direcciones para los BFS.
    random_dirs();

    //Reccorro las unidades para ver si estan en situación extrema.
    VI alive = alive_units(me());
    for (ID id : alive) {
      Pos pos_unit = unit(id).pos;

      Dir move_to_do;
      Pos ipoint;
      int prio;
      bool ip_ocuped = true;
      if (extreme_situation(pos_unit, ipoint, move_to_do, prio, ip_ocuped)) {
        if (not ip_ocuped) {final_tasks.insert(make_pair(id, make_pair(move_to_do,MID_PRIO))); ip_ocuped = true;}
        else {
          set_pos_it it = pos_assigned.find(ipoint);
          if (it == pos_assigned.end()) {
            interest_points.erase(ipoint);
            pos_assigned.insert(ipoint);
            units_assigned.insert(id);
            final_tasks.insert(make_pair(id, make_pair(move_to_do,prio)));
          }
        }
      }
      //Waste imaginario para las posiciones que impliquen ser mordido por un zombie.
      if (unit(id).rounds_for_zombie == -1) waste_danger_cells(pos_unit);
    }
    
    //BFS desde los puntos de interés.
    TK tasks;
    set_pos_it it = interest_points.begin();
    while (it != interest_points.end()) {
      BFS_from_ipoints(*it, tasks);
      ++it;
    }
    
    //Voy sacando los elementos de la cola de prioridad de tareas, si o la unidad ya tiene tarea o la posicion ya está asignada, lo ignoro.
    while (not tasks.empty()) {
      ID id = tasks.top().second.first;
      Pos p = tasks.top().second.second.first;
      Dir move_to_do = tasks.top().second.second.second;
      tasks.pop();

      if (not already_assigned(id, p, units_assigned, pos_assigned)) {
        //Si un enemigo puede ir a la celda a la que vamos, és preferible hacerlo con prioridad mínima, así él se mueve primero y aprovechamos el 30%!!
        if (enemy_will_go(unit(id).pos + move_to_do) and unit(id).rounds_for_zombie == -1) final_tasks.insert(make_pair(id, make_pair(move_to_do, MIN_PRIO)));
        else final_tasks.insert(make_pair(id, make_pair(move_to_do, MID_PRIO)));
        pos_assigned.insert(p);
        units_assigned.insert(id);
      }
    }

    priority_queue<order> orders;

    //Segundo recorrido de mis unidades, si no tienen tarea se lanza un BFS a por celdas, si estan muy lejos se quedan quietas.
    for (ID id : alive) {
      map<ID,pair<Dir,int>>::const_iterator map_it = final_tasks.find(id);
      if (map_it != final_tasks.end()) {
        orders.push(make_pair(map_it->second.second, make_pair(id, map_it->second.first)));
      }
      else {
        Pos pos_unit = unit(id).pos;
        Dir move_to_do;
        if (not BFS_to_EStreet(pos_unit, move_to_do)) orders.push(make_pair(MID_PRIO, make_pair(id, ILEGAL_MOVE)));
        else orders.push(make_pair(MID_PRIO, make_pair(id, move_to_do)));
      }
    }

    //Voy sacando los elementos de la cola de prioridad para hacer los movimientos en ORDEN.
    while (not orders.empty()) {
      ID id = orders.top().second.first;
      Dir d = orders.top().second.second;
      move(id, d);
      orders.pop();
    }
  }


  /**
   * Play method, invoked once per each round.
   */
  virtual void play () {
    //Si me paso de tiempo de CPU, reduzco la profundidad del BFS que más consume.
    double st = status(me());
    if (st >= 0.95) return;
    if (st >= 0.85) MAX_PROF = 20;
    //A partir de la ronda 190 mis unidades irán a por lo mas cercano, independientemente de lo que sea.
    if (round() >= 185) crazy_mode = true;
    move_units();
  }

};


/**
 * Do not modify the following line.
 */
RegisterPlayer(PLAYER_NAME);
